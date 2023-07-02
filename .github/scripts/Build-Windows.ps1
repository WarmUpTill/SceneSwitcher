[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('x86', 'x64')]
    [string] $Target,
    [ValidateSet('Visual Studio 17 2022', 'Visual Studio 16 2019')]
    [string] $CMakeGenerator,
    [switch] $SkipAll,
    [switch] $SkipBuild,
    [switch] $SkipDeps,
    [switch] $SkipUnpack
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $PSVersionTable.PSVersion -lt '7.0.0' ) {
    Write-Warning 'The obs-deps PowerShell build script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Build {
    trap {
        Pop-Location -Stack BuildTemp -ErrorAction 'SilentlyContinue'
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach ($Utility in $UtilityFunctions) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $script:DepsVersion = ''
    $script:QtVersion = '5'
    $script:VisualStudioVersion = ''
    $script:PlatformSDK = '10.0.18363.657'

    Setup-Host

    if ( $CmakeGenerator -eq '' ) {
        $CmakeGenerator = $script:VisualStudioVersion
    }

    $DepsPath = "plugin-deps-${script:DepsVersion}-qt${script:QtVersion}-${script:Target}"
    $DepInstallPath = "$(Resolve-Path -Path ${ProjectRoot}/../obs-build-dependencies/${DepsPath})"

    $OpenCVPath = "${ProjectRoot}/deps/opencv"
    $OpenCVBuildPath = "${OpenCVPath}/build"

    Push-Location -Stack BuildOpenCVTemp
    if ( ! ( ( $SkipAll ) -or ( $SkipBuild ) ) ) {
        Ensure-Location $ProjectRoot

        $OpenCVCmakeArgs = @(
            '-G', $CmakeGenerator
            "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
            "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_PREFIX_PATH:PATH=${DepInstallPath}"
            "-DBUILD_LIST=core,imgproc,objdetect"
        )

        Log-Information "Configuring OpenCV..."
        Invoke-External cmake -S ${OpenCVPath} -B ${OpenCVBuildPath} @OpenCVCmakeArgs

        $OpenCVCmakeArgs = @(
            '--config', "Release"
        )

        if ( $VerbosePreference -eq 'Continue' ) {
            $OpenCVCmakeArgs += ('--verbose')
        }

        Log-Information "Building OpenCV..."
        Invoke-External cmake --build "${OpenCVBuildPath}" @OpenCVCmakeArgs
    }
    Log-Information "Install OpenCV}..."
    Invoke-External cmake --install "${OpenCVBuildPath}" --prefix "${DepInstallPath}" @OpenCVCmakeArgs

    $LeptonicaPath = "${ProjectRoot}/deps/leptonica"
    $LeptonicaBuildPath = "${LeptonicaPath}/build"

    Push-Location -Stack BuildLeptonicaTemp
    if ( ! ( ( $SkipAll ) -or ( $SkipBuild ) ) ) {
        Ensure-Location $ProjectRoot

        $LeptonicaCmakeArgs = @(
            '-G', $CmakeGenerator
            "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
            "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
            "-DCMAKE_BUILD_TYPE=${Configuration}"
            "-DCMAKE_PREFIX_PATH:PATH=${DepInstallPath}"
            "-DCMAKE_INSTALL_PREFIX:PATH=${DepInstallPath}"
            "-DSW_BUILD=OFF"
            "-DLIBWEBP_SUPPORT=OFF"
        )

        Log-Information "Configuring leptonica..."
        Invoke-External cmake -S ${LeptonicaPath} -B ${LeptonicaBuildPath} @LeptonicaCmakeArgs

        $LeptonicaCmakeArgs = @(
            '--config', "${Configuration}"
        )

        if ( $VerbosePreference -eq 'Continue' ) {
            $LeptonicaCmakeArgs += ('--verbose')
        }

        Log-Information "Building leptonica..."
        Invoke-External cmake --build "${LeptonicaBuildPath}" @LeptonicaCmakeArgs
    }
    Log-Information "Install leptonica..."
    Invoke-External cmake --install "${LeptonicaBuildPath}" --prefix "${DepInstallPath}" @LeptonicaCmakeArgs

    Push-Location -Stack BuildTesseractTemp
    if ( ! ( ( $SkipAll ) -or ( $SkipBuild ) ) ) {
        Ensure-Location $ProjectRoot

        $TesseractPath = "${ProjectRoot}/deps/tesseract"
        $TesseractBuildPath = "${TesseractPath}/build"

        # Explicitly disable PkgConfig and tiff as it will lead build errors
        $TesseractCmakeArgs = @(
            '-G', $CmakeGenerator
            "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
            "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
            "-DCMAKE_BUILD_TYPE=${Configuration}"
            "-DCMAKE_PREFIX_PATH:PATH=${DepInstallPath}"
            "-DCMAKE_INSTALL_PREFIX:PATH=${DepInstallPath}"
            "-DSW_BUILD=OFF"
            "-DDISABLE_CURL=ON"
            "-DBUILD_TRAINING_TOOLS=OFF"
            "-DCMAKE_DISABLE_FIND_PACKAGE_TIFF=TRUE"
            "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE"
        )

        Log-Information "Configuring tesseract..."
        Invoke-External cmake -S ${TesseractPath} -B ${TesseractBuildPath} @TesseractCmakeArgs

        $TesseractCmakeArgs = @(
            '--config', "${Configuration}"
        )

        if ( $VerbosePreference -eq 'Continue' ) {
            $TesseractCmakeArgs += ('--verbose')
        }

        Log-Information "Building tesseract..."
        Invoke-External cmake --build "${TesseractBuildPath}" @TesseractCmakeArgs
    }
    Log-Information "Install tesseract..."
    Invoke-External cmake --install "${TesseractBuildPath}" --prefix "${DepInstallPath}" @TesseractCmakeArgs

    (Get-Content -Path ${ProjectRoot}/CMakeLists.txt -Raw) `
        -replace "project\((.*) VERSION (.*)\)", "project(${ProductName} VERSION ${ProductVersion})" `
    | Out-File -Path ${ProjectRoot}/CMakeLists.txt

    Setup-Obs

    Push-Location -Stack BuildTemp
    if ( ! ( ( $SkipAll ) -or ( $SkipBuild ) ) ) {
        Ensure-Location $ProjectRoot

        $CmakeArgs = @(
            '-G', $CmakeGenerator
            "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
            "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
            "-DCMAKE_BUILD_TYPE=${Configuration}"
            "-DCMAKE_PREFIX_PATH:PATH=${DepInstallPath}"
            "-DQT_VERSION=${script:QtVersion}"
        )

        Log-Debug "Attempting to configure OBS with CMake arguments: $($CmakeArgs | Out-String)"
        Log-Information "Configuring ${ProductName}..."
        Invoke-External cmake -S . -B build_${script:Target} @CmakeArgs

        $CmakeArgs = @(
            '--config', "${Configuration}"
        )

        if ( $VerbosePreference -eq 'Continue' ) {
            $CmakeArgs += ('--verbose')
        }

        Log-Information "Building ${ProductName}..."
        Invoke-External cmake --build "build_${script:Target}" @CmakeArgs
    }
    Log-Information "Install ${ProductName}..."
    Invoke-External cmake --install "build_${script:Target}" --prefix "${ProjectRoot}/release" @CmakeArgs

    Pop-Location -Stack BuildTemp
}

Build
