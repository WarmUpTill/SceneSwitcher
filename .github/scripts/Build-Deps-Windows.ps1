[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('x86', 'x64')]
    [string] $Target,
    [ValidateSet('Visual Studio 17 2022', 'Visual Studio 16 2019')]
    [string] $CMakeGenerator,
    [string] $OutDirName,
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

    $script:VisualStudioVersion = ''
    $script:PlatformSDK = '10.0.18363.657'

    Setup-Host

    if ( $CmakeGenerator -eq '' ) {
        $CmakeGenerator = $script:VisualStudioVersion
    }

    $DepsPath = "plugin-deps-${script:DepsVersion}-*-${script:Target}"
    $OBSDepPath = "$(Resolve-Path -Path ${ProjectRoot}/../obs-build-dependencies/${DepsPath})"

    if ( $OutDirName -eq '' ) {
        $OutDirName = "advss-build-dependencies"
    }
    New-Item -ItemType Directory -Force -Path ${ProjectRoot}/../${OutDirName}

    $ADVSSDepPath = "$(Resolve-Path -Path ${ProjectRoot}/../${OutDirName})"

    $OpenCVPath = "${ProjectRoot}/deps/opencv"
    $OpenCVBuildPath = "${OpenCVPath}/build"

    Push-Location -Stack BuildOpenCVTemp
    Ensure-Location $ProjectRoot

    $OpenCVCmakeArgs = @(
        '-G', $CmakeGenerator
        "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
        "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_PREFIX_PATH:PATH=${OBSDepPath}"
        "-DCMAKE_INSTALL_PREFIX:PATH=${ADVSSDepPath}"
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
    Log-Information "Install OpenCV}..."
    Invoke-External cmake --install "${OpenCVBuildPath}" --prefix "${ADVSSDepPath}" @OpenCVCmakeArgs

    $LeptonicaPath = "${ProjectRoot}/deps/leptonica"
    $LeptonicaBuildPath = "${LeptonicaPath}/build"

    Push-Location -Stack BuildLeptonicaTemp
    Ensure-Location $ProjectRoot

    $LeptonicaCmakeArgs = @(
        '-G', $CmakeGenerator
        "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
        "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
        "-DCMAKE_BUILD_TYPE=${Configuration}"
        "-DCMAKE_PREFIX_PATH:PATH=${OBSDepPath}"
        "-DCMAKE_INSTALL_PREFIX:PATH=${ADVSSDepPath}"
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

    Log-Information "Install leptonica..."
    Invoke-External cmake --install "${LeptonicaBuildPath}" --prefix "${ADVSSDepPath}" @LeptonicaCmakeArgs

    Push-Location -Stack BuildTesseractTemp
    Ensure-Location $ProjectRoot

    $TesseractPath = "${ProjectRoot}/deps/tesseract"
    $TesseractBuildPath = "${TesseractPath}/build"

    # Explicitly disable PkgConfig and tiff as it will lead build errors
    $TesseractCmakeArgs = @(
        '-G', $CmakeGenerator
        "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
        "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
        "-DCMAKE_BUILD_TYPE=${Configuration}"
        "-DCMAKE_PREFIX_PATH:PATH=${OBSDepPath}"
        "-DCMAKE_INSTALL_PREFIX:PATH=${ADVSSDepPath}"
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

    Log-Information "Install tesseract..."
    Invoke-External cmake --install "${TesseractBuildPath}" --prefix "${ADVSSDepPath}" @TesseractCmakeArgs
}

Build
