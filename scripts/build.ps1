. "$PSScriptRoot\toolpath.ps1"
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
if (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
    mingw32-make -f app.mk
} else {
    make -f app.mk
}
