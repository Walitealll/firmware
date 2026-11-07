$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $projectRoot
. "$PSScriptRoot\toolpath.ps1"

# Create the nested directory natively: the generated Makefile uses plain mkdir.
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'build\bno055-reference') -Force | Out-Null
$referenceArgs = @('-f', 'bno055-reference.mk', 'BUILD_DIR=build/bno055-reference', 'TARGET=firmware-bno055-reference', 'all')
if (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
    & mingw32-make @referenceArgs
} else {
    & make @referenceArgs
}
exit $LASTEXITCODE
