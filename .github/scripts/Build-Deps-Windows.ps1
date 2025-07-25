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

if (-not $Target) {
    $Target = "x64"
}

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

    if ( ! $SkipDeps ) {
        Install-BuildDependencies -WingetFile "${ScriptHome}/.Wingetfile"
    }

    Log-Information "Run plugin configure step to download OBS deps ..."
    Push-Location -Stack BuildTemp
    if ( ! ( ( $SkipAll ) -or ( $SkipBuild ) ) ) {
        Ensure-Location $ProjectRoot

        $CmakeArgs = @(
            "-DCMAKE_PREFIX_PATH:PATH=${ADVSSDepPath}"
        )
        $CmakeBuildArgs = @()
        $CmakeInstallArgs = @()

        if ( $VerbosePreference -eq 'Continue' ) {
            $CmakeBuildArgs += ('--verbose')
            $CmakeInstallArgs += ('--verbose')
        }

        if ( $DebugPreference -eq 'Continue' ) {
            $CmakeArgs += ('--debug-output')
        }

        $Preset = "windows-$(if ( $Env:CI -ne $null ) { 'ci-' })${Target}"

        $CmakeArgs += @(
            '--preset', $Preset
        )

        $CmakeBuildArgs += @(
            '--build'
            '--preset', $Preset
            '--config', $Configuration
            '--parallel'
            '--', '/consoleLoggerParameters:Summary', '/noLogo'
        )

        $CmakeInstallArgs += @(
            '--install', "build_${Target}"
            '--prefix', "${ProjectRoot}/release/${Configuration}"
            '--config', $Configuration
        )

        Log-Group "Configuring ${ProductName}..."
        Invoke-External cmake @CmakeArgs
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $depsVersion = $BuildSpec.dependencies.prebuilt.version
    $qtDepsVersion = $BuildSpec.dependencies.qt6.version
    $OBSDepPath = "$(Resolve-Path -Path ${ProjectRoot}/.deps/obs-deps-${depsVersion}-${Target});$(Resolve-Path -Path ${ProjectRoot}/.deps/obs-deps-qt6-${qtDepsVersion}-${Target})"

    if ( $OutDirName -eq '' ) {
        $OutDirName = ".deps/advss-build-dependencies"
    }
    New-Item -ItemType Directory -Force -Path ${ProjectRoot}/${OutDirName}

    $ADVSSDepPath = "$(Resolve-Path -Path ${ProjectRoot}/${OutDirName})"

    $OpenCVPath = "${ProjectRoot}/deps/opencv"
    $OpenCVBuildPath = "${OpenCVPath}/build"

    Push-Location -Stack BuildOpenCVTemp
    Ensure-Location $ProjectRoot

    $OpenCVCmakeArgs = @(
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

    Push-Location -Stack BuildLibusbTemp
    Ensure-Location $ProjectRoot

    $LibusbPath = "${ProjectRoot}/deps/libusb"

    Log-Information "Building libusb..."
    $msbuildExe = vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | select-object -first 1

    if ($msbuildExe) {
        $env:CL="/wd5287"
        Invoke-External $msbuildExe "${LibusbPath}/msvc/libusb.sln" /property:Configuration=Release /property:Platform=x64
        Remove-Item Env:CL

        $libusbBuildResultDirectory = "${LibusbPath}/build/v143/x64/Release"
        if (-not (Test-Path -Path $libusbBuildResultDirectory)) {
            $libusbBuildResultDirectory = "${LibusbPath}/x64/Release/dll"
        }
        Copy-Item -Path "${libusbBuildResultDirectory}/*" -Destination ${ADVSSDepPath} -Recurse -Force
    } else {
        Log-Information "Failed to locate msbuild.exe - skipping libusb build"
    }

    Push-Location -Stack BuildMqttTemp
    Ensure-Location $ProjectRoot

    $MqttPath = "${ProjectRoot}/deps/paho.mqtt.cpp"
    $MqttBuildPath = "${MqttPath}/build"

    # Explicitly disable PkgConfig and tiff as it will lead build errors
    $MqttCmakeArgs = @(
        "-DCMAKE_BUILD_TYPE=${Configuration}"
        "-DCMAKE_PREFIX_PATH:PATH=${OBSDepPath}"
        "-DCMAKE_INSTALL_PREFIX:PATH=${ADVSSDepPath}"
        "-DPAHO_WITH_MQTT_C=ON"
    )

    Log-Information "Configuring paho.mqtt.cpp..."
    Invoke-External cmake -S ${MqttPath} -B ${MqttBuildPath} @MqttCmakeArgs

    $MqttCmakeArgs = @(
        '--config', "${Configuration}"
    )

    if ( $VerbosePreference -eq 'Continue' ) {
        $MqttCmakeArgs += ('--verbose')
    }

    Log-Information "Building paho.mqtt.cpp..."
    Invoke-External cmake --build "${MqttBuildPath}" @MqttCmakeArgs

    Log-Information "Install paho.mqtt.cpp..."
    Invoke-External cmake --install "${MqttBuildPath}" --prefix "${ADVSSDepPath}" @MqttCmakeArgs
}

Build
