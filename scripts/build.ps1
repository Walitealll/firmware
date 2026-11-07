param([ValidateSet('all', 'clean')][string]$Target = 'all')

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $projectRoot

if ($Target -eq 'clean') {
    $buildOutput = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))
    if ((Split-Path -Parent $buildOutput) -ne $projectRoot) {
        throw 'Build output is outside the project directory.'
    }
    if (Test-Path -LiteralPath $buildOutput) {
        if ((Get-Item -LiteralPath $buildOutput).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Refusing to clean a linked build directory.'
        }
        Remove-Item -LiteralPath $buildOutput -Recurse -Force
    }
    exit 0
}

. "$PSScriptRoot\toolpath.ps1"
if (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
    mingw32-make -f app.mk $Target
} else {
    make -f app.mk $Target
}
exit $LASTEXITCODE
