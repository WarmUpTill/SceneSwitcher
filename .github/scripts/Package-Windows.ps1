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
    $PluginPath = "${ReleasePath}/${ProductName}"
    $NewDataPath = "${PluginPath}/data"
    $CIWindowsDir = "${ProjectRoot}/build-aux/CI/windows"

    # Binaries are in bin/64bit if built with ADVSS_WINDOWS_LEGACY_LAYOUT
    $LegacyBinPath = "${PluginPath}/bin/64bit"
    if ( Test-Path -Path $LegacyBinPath ) {
        $BinItems = Get-ChildItem -Path $LegacyBinPath
    } else {
        $BinItems = Get-ChildItem -Path $PluginPath -Exclude 'data' -ErrorAction SilentlyContinue
    }

    function Copy-PluginFiles {
        param(
            [string] $Destination,
            [string] $BinDir,
            [string] $DataDir
        )

        if ( $BinItems ) {
            New-Item -ItemType Directory -Force -Path "${Destination}/${BinDir}" | Out-Null
            $BinItems | Copy-Item -Destination "${Destination}/${BinDir}" -Recurse -Force
        }
        if ( Test-Path -Path $NewDataPath ) {
            New-Item -ItemType Directory -Force -Path "${Destination}/${DataDir}" | Out-Null
            Copy-Item -Path "${NewDataPath}/*" -Destination "${Destination}/${DataDir}" -Recurse -Force
        }
    }

    function New-PluginArchive {
        param(
            [string] $Suffix,
            [string] $Readme,
            [string] $BinDir,
            [string] $DataDir
        )

        Log-Group "Archiving ${OutputName}${Suffix}.zip..."
        $Staging = "${ProjectRoot}/release/zip-staging"
        Remove-Item -Path $Staging -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $Staging | Out-Null
        Copy-Item -Path "${CIWindowsDir}/${Readme}" -Destination "${Staging}/README.txt"
        Copy-PluginFiles -Destination $Staging -BinDir $BinDir -DataDir $DataDir
        Compress-Archive -Force -Path (Get-ChildItem -Path $Staging) `
            -CompressionLevel Optimal `
            -DestinationPath "${ProjectRoot}/release/${OutputName}${Suffix}.zip"
        Remove-Item -Path $Staging -Recurse -Force
        Log-Group
    }

    # OBS 33 and newer, extract to %ProgramData%\obs-studio\plugins\
    New-PluginArchive -Suffix '' -Readme 'README.txt' `
        -BinDir $ProductName -DataDir "${ProductName}/data"

    # OBS 28 to 32, extract to %ProgramData%\obs-studio\plugins\
    New-PluginArchive -Suffix '-obs32' -Readme 'README-obs32.txt' `
        -BinDir "${ProductName}/bin/64bit" -DataDir "${ProductName}/data"

    # OBS 32 and older, extract to the OBS installation directory
    New-PluginArchive -Suffix '-portable-obs32' -Readme 'README-portable-obs32.txt' `
        -BinDir 'obs-plugins/64bit' -DataDir "data/obs-plugins/${ProductName}"

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

        # The installer places these files according to the detected layout
        Copy-PluginFiles -Destination 'Package' -BinDir 'bin' -DataDir 'data'

        Invoke-External iscc ${IsccFile} /O"${ProjectRoot}/release" /F"${OutputName}-Installer"
        Remove-Item -Path Package -Recurse
        Pop-Location -Stack BuildTemp

        Log-Group
    }
}

Package
