[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [switch] $BuildInstaller,
    [switch] $SkipDeps
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}


if ( $PSVersionTable.PSVersion -lt '7.0.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Pop-Location -Stack BuildTemp -ErrorAction 'SilentlyContinue'
        Write-Error $_
        Log-Group
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $GitOutput = git describe --tags
    Log-Information "Using git tag as version identifier '${GitOutput}'"
    $ProductVersion = $GitOutput

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    if ( ! $SkipDeps ) {
        Install-BuildDependencies -WingetFile "${ScriptHome}/.Wingetfile"
    }

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    $ReleasePath = "${ProjectRoot}/release/${Configuration}"
    $NewBinPath  = "${ReleasePath}/${ProductName}/bin/64bit"
    $NewDataPath = "${ReleasePath}/${ProductName}/data"
    $CIWindowsDir = "${ProjectRoot}/build-aux/CI/windows"

    # --- Recommended zip (new layout, extract to %ProgramData%\obs-studio\) ---
    Log-Group "Archiving ${ProductName} (recommended)..."
    $RecStaging = "${ProjectRoot}/release/zip-staging-rec"
    Remove-Item -Path $RecStaging -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $RecStaging | Out-Null
    Copy-Item -Path "${CIWindowsDir}/README.txt" -Destination "${RecStaging}/README.txt"
    if ( Test-Path -Path $NewBinPath ) {
        $RecBinPath = "${RecStaging}/${ProductName}/bin/64bit"
        New-Item -ItemType Directory -Force -Path $RecBinPath | Out-Null
        Copy-Item -Path "${NewBinPath}/*" -Destination $RecBinPath -Recurse -Force
    }
    if ( Test-Path -Path $NewDataPath ) {
        $RecDataPath = "${RecStaging}/${ProductName}/data"
        New-Item -ItemType Directory -Force -Path $RecDataPath | Out-Null
        Copy-Item -Path "${NewDataPath}/*" -Destination $RecDataPath -Recurse -Force
    }
    Compress-Archive -Force -Path (Get-ChildItem -Path $RecStaging) `
        -CompressionLevel Optimal `
        -DestinationPath "${ProjectRoot}/release/${OutputName}.zip"
    Remove-Item -Path $RecStaging -Recurse -Force
    Log-Group

    # --- Legacy zip (old layout, extract to OBS install directory) ---
    Log-Group "Archiving ${ProductName} (legacy)..."
    $LegStaging = "${ProjectRoot}/release/zip-staging-leg"
    Remove-Item -Path $LegStaging -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $LegStaging | Out-Null
    Copy-Item -Path "${CIWindowsDir}/README-legacy.txt" -Destination "${LegStaging}/README.txt"
    if ( Test-Path -Path $NewBinPath ) {
        $LegBinPath = "${LegStaging}/obs-plugins/64bit"
        New-Item -ItemType Directory -Force -Path $LegBinPath | Out-Null
        Copy-Item -Path "${NewBinPath}/*" -Destination $LegBinPath -Recurse -Force
    }
    if ( Test-Path -Path $NewDataPath ) {
        $LegDataPath = "${LegStaging}/data/obs-plugins/${ProductName}"
        New-Item -ItemType Directory -Force -Path $LegDataPath | Out-Null
        Copy-Item -Path "${NewDataPath}/*" -Destination $LegDataPath -Recurse -Force
    }
    Compress-Archive -Force -Path (Get-ChildItem -Path $LegStaging) `
        -CompressionLevel Optimal `
        -DestinationPath "${ProjectRoot}/release/${OutputName}-legacy.zip"
    Remove-Item -Path $LegStaging -Recurse -Force
    Log-Group

    if ( ( $BuildInstaller ) ) {
        Log-Group "Packaging ${ProductName}..."

        $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"
        if ( ! ( Test-Path -Path $IsccFile ) ) {
            throw 'InnoSetup install script not found. Run the build script or the CMake build and install procedures first.'
        }

        Log-Information 'Creating InnoSetup installer...'
        Push-Location -Stack BuildTemp
        Ensure-Location -Path "${ProjectRoot}/release"
        Remove-Item -Path Package -Recurse -Force -ErrorAction SilentlyContinue

        # Recommended layout (for %ProgramData%\obs-studio\plugins\)
        $PkgRec = "Package/recommended"
        New-Item -ItemType Directory -Force -Path $PkgRec | Out-Null
        Copy-Item -Path "${Configuration}/*" -Destination $PkgRec -Recurse -Force

        # Legacy layout (for OBS installation directory)
        if ( Test-Path "${Configuration}/${ProductName}/bin/64bit" ) {
            $PkgLegBin = "Package/legacy/obs-plugins/64bit"
            New-Item -ItemType Directory -Force -Path $PkgLegBin | Out-Null
            Copy-Item -Path "${Configuration}/${ProductName}/bin/64bit/*" -Destination $PkgLegBin -Recurse -Force
        }
        if ( Test-Path "${Configuration}/${ProductName}/data" ) {
            $PkgLegData = "Package/legacy/data/obs-plugins/${ProductName}"
            New-Item -ItemType Directory -Force -Path $PkgLegData | Out-Null
            Copy-Item -Path "${Configuration}/${ProductName}/data/*" -Destination $PkgLegData -Recurse -Force
        }

        Invoke-External iscc ${IsccFile} /O"${ProjectRoot}/release" /F"${OutputName}-Installer"
        Remove-Item -Path Package -Recurse
        Pop-Location -Stack BuildTemp

        Log-Group
    }
}

Package
