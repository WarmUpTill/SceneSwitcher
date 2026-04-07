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

    # Add legacy folder structure alongside the recommended one so both are
    # included in the zip archive.  Users on older OBS versions can still
    # copy files from obs-plugins/64bit and data/obs-plugins/<name>/ while
    # new installations use the recommended plugins/<name>/bin|data layout.
    $ReleasePath   = "${ProjectRoot}/release/${Configuration}"
    $LegacyBinPath = "${ReleasePath}/obs-plugins/64bit"
    $LegacyDataPath = "${ReleasePath}/data/obs-plugins/${ProductName}"
    $NewBinPath    = "${ReleasePath}/${ProductName}/bin/64bit"
    $NewDataPath   = "${ReleasePath}/${ProductName}/data"

    if ( Test-Path -Path $NewBinPath ) {
        New-Item -ItemType Directory -Force -Path $LegacyBinPath | Out-Null
        Copy-Item -Path "${NewBinPath}/*" -Destination $LegacyBinPath -Recurse -Force
    }
    if ( Test-Path -Path $NewDataPath ) {
        New-Item -ItemType Directory -Force -Path $LegacyDataPath | Out-Null
        Copy-Item -Path "${NewDataPath}/*" -Destination $LegacyDataPath -Recurse -Force
    }

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
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
        Copy-Item -Path ${Configuration} -Destination Package -Recurse
        # Remove the legacy paths from the installer package — the installer
        # targets the recommended layout only.
        Remove-Item -Path "Package/obs-plugins" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -Path "Package/data"        -Recurse -Force -ErrorAction SilentlyContinue
        Invoke-External iscc ${IsccFile} /O"${ProjectRoot}/release" /F"${OutputName}-Installer"
        Remove-Item -Path Package -Recurse
        Pop-Location -Stack BuildTemp

        Log-Group
    }
}

Package
