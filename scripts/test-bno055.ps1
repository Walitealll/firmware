param([string]$Clang = 'clang.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command $Clang -ErrorAction Stop).Source
$linker = Join-Path (Split-Path -Parent $compiler) 'lld.exe'
if (-not (Test-Path -LiteralPath $linker)) {
    throw "lld.exe is required alongside clang.exe. Pass -Clang with an LLVM installation path."
}
$output = Join-Path $root 'build\tests-bno055'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$sources = @('tests/test_bno055.c', 'App/FW_angle.c', 'App/FW_sync.c', 'App/FW_protocol.c', 'Drivers/BNO055/bno055.c', 'Drivers/BNO055/bno055_port.c')
$objects = @()
Push-Location $root
try {
    foreach ($source in $sources) {
        $object = Join-Path $output (([IO.Path]::GetFileNameWithoutExtension($source)) + '.obj')
        & $compiler --target=x86_64-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -O1 -std=c11 -Wall -Wextra -Werror -Itests/host -IApp -Iconfig -IDrivers/BNO055 -c $source -o $object
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
        $objects += $object
    }
    $test = Join-Path $output 'test_bno055.exe'
    & $linker -flavor link /entry:main /subsystem:console /nodefaultlib "/out:$test" @objects
    if ($LASTEXITCODE -ne 0) { throw 'Test linking failed' }
    & $test
    if ($LASTEXITCODE -ne 0) { throw "BNO055 regression failed at tests/test_bno055.c line $LASTEXITCODE" }
    Write-Output 'PASS: BNO055 software reset, heading wrap, packet 0x03, two buses, clocks and I2C error recovery.'
} finally {
    Pop-Location
}
