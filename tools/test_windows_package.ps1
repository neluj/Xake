param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDirectory
)

$ErrorActionPreference = "Stop"

$resolvedPackage = (Resolve-Path -LiteralPath $PackageDirectory).Path
$executables = @(Get-ChildItem -LiteralPath $resolvedPackage -Filter "Xake.exe" -File -Recurse)
if ($executables.Count -ne 1) {
    throw "Expected one Xake.exe in '$resolvedPackage', found $($executables.Count)."
}

$savedEnvironment = @{
    Path = $env:Path
    QT_PLUGIN_PATH = $env:QT_PLUGIN_PATH
    QML2_IMPORT_PATH = $env:QML2_IMPORT_PATH
    QT_QPA_PLATFORM_PLUGIN_PATH = $env:QT_QPA_PLATFORM_PLUGIN_PATH
    APPDATA = $env:APPDATA
    LOCALAPPDATA = $env:LOCALAPPDATA
}

$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$isolatedProfile = [System.IO.Path]::GetFullPath(
    (Join-Path $tempRoot (
        "xake-package-smoke-" + [Guid]::NewGuid().ToString("N"))))
if (-not $isolatedProfile.StartsWith(
        $tempRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create an isolated profile outside the temporary directory."
}
New-Item -ItemType Directory -Path $isolatedProfile | Out-Null

try {
    $env:Path = "$env:SystemRoot;$env:SystemRoot\System32"
    $env:QT_PLUGIN_PATH = $null
    $env:QML2_IMPORT_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    $env:APPDATA = $isolatedProfile
    $env:LOCALAPPDATA = $isolatedProfile

    $process = Start-Process `
        -FilePath $executables[0].FullName `
        -ArgumentList "--smoke-test" `
        -WorkingDirectory $executables[0].DirectoryName `
        -WindowStyle Hidden `
        -PassThru

    if (-not $process.WaitForExit(15000)) {
        $process.Kill()
        throw "Packaged Xake did not finish its smoke test within 15 seconds."
    }
    if ($process.ExitCode -ne 0) {
        throw "Packaged Xake smoke test exited with code $($process.ExitCode)."
    }

    Write-Host "Package smoke test passed without Qt or MinGW in PATH."
} finally {
    $env:Path = $savedEnvironment.Path
    $env:QT_PLUGIN_PATH = $savedEnvironment.QT_PLUGIN_PATH
    $env:QML2_IMPORT_PATH = $savedEnvironment.QML2_IMPORT_PATH
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $savedEnvironment.QT_QPA_PLATFORM_PLUGIN_PATH
    $env:APPDATA = $savedEnvironment.APPDATA
    $env:LOCALAPPDATA = $savedEnvironment.LOCALAPPDATA

    if (Test-Path -LiteralPath $isolatedProfile) {
        $resolvedProfile =
            (Resolve-Path -LiteralPath $isolatedProfile).Path
        if (-not $resolvedProfile.StartsWith(
                $tempRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a directory outside the temporary directory."
        }
        Remove-Item -LiteralPath $resolvedProfile -Recurse -Force
    }
}
