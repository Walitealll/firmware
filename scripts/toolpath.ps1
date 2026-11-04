function Add-DirIfToolMissing([string]$exeName, [string[]]$patterns) {
    if (Get-Command $exeName -ErrorAction SilentlyContinue) {
        return
    }
    foreach ($pattern in $patterns) {
        $hit = Get-Item -Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) {
            $dir = if ($hit.PSIsContainer) { $hit.FullName } else { $hit.DirectoryName }
            $env:Path = "$dir;$env:Path"
            return
        }
    }
    throw "$exeName не найден. Поставь программу и включи галочку Add to PATH в её установщике."
}

Add-DirIfToolMissing 'arm-none-eabi-gcc.exe' @(
    "$env:ProgramFiles\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe",
    "${env:ProgramFiles(x86)}\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe"
)

Add-DirIfToolMissing 'openocd.exe' @(
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack*\xpack-openocd-*\bin\openocd.exe",
    "$env:ProgramFiles\OpenOCD\bin\openocd.exe",
    "${env:ProgramFiles(x86)}\OpenOCD\bin\openocd.exe"
)
