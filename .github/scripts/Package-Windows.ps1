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

    # Build a staging directory with two subfolders:
    #   recommended/ - new layout, extract to %ProgramData%\obs-studio\
    #   legacy/      - old layout, extract to the OBS install directory
    $ReleasePath  = "${ProjectRoot}/release/${Configuration}"
    $StagingPath  = "${ProjectRoot}/release/zip-staging"
    $NewBinPath   = "${ReleasePath}/${ProductName}/bin/64bit"
    $NewDataPath  = "${ReleasePath}/${ProductName}/data"

    Remove-Item -Path $StagingPath -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $StagingPath | Out-Null
    Copy-Item -Path "${ProjectRoot}/build-aux/CI/windows/README.txt" -Destination "${StagingPath}/README.txt"

    if ( Test-Path -Path $NewBinPath ) {
        $RecBinPath    = "${StagingPath}/recommended/${ProductName}/bin/64bit"
        $LegacyBinPath = "${StagingPath}/legacy/obs-plugins/64bit"
        New-Item -ItemType Directory -Force -Path $RecBinPath    | Out-Null
        New-Item -ItemType Directory -Force -Path $LegacyBinPath | Out-Null
        Copy-Item -Path "${NewBinPath}/*" -Destination $RecBinPath    -Recurse -Force
        Copy-Item -Path "${NewBinPath}/*" -Destination $LegacyBinPath -Recurse -Force
    }
    if ( Test-Path -Path $NewDataPath ) {
        $RecDataPath    = "${StagingPath}/recommended/${ProductName}/data"
        $LegacyDataPath = "${StagingPath}/legacy/data/obs-plugins/${ProductName}"
        New-Item -ItemType Directory -Force -Path $RecDataPath    | Out-Null
        New-Item -ItemType Directory -Force -Path $LegacyDataPath | Out-Null
        Copy-Item -Path "${NewDataPath}/*" -Destination $RecDataPath    -Recurse -Force
        Copy-Item -Path "${NewDataPath}/*" -Destination $LegacyDataPath -Recurse -Force
    }

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path $StagingPath)
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($null -ne $Env:CI)
    }
    Compress-Archive -Force @CompressArgs
    Remove-Item -Path $StagingPath -Recurse -Force
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
        Invoke-External iscc ${IsccFile} /O"${ProjectRoot}/release" /F"${OutputName}-Installer"
        Remove-Item -Path Package -Recurse
        Pop-Location -Stack BuildTemp

        Log-Group
    }
}

Package
