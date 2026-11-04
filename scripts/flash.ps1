. "$PSScriptRoot\toolpath.ps1"
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/firmware.elf verify reset exit"
