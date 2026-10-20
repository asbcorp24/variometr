param(
  [string]$Compiler = '',
  [string]$Environment = 'esp32doit-devkit-v1',
  [switch]$Screens
)
$ErrorActionPreference = 'Stop'
if (-not $Compiler) {
  $taskGcc = Get-Command g++ -ErrorAction SilentlyContinue
  if ($taskGcc) { $Compiler = $taskGcc.Source }
  elseif (Test-Path 'C:/Strawberry/c/bin/g++.exe') { $Compiler = 'C:/Strawberry/c/bin/g++.exe' }
  else { throw 'Install g++ or pass -Compiler with its full path.' }
}
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location $taskRoot
try {
  New-Item -ItemType Directory -Path .pio -Force | Out-Null
  function Invoke-HostTest([string]$Name, [string[]]$Sources, [string[]]$Extra = @()) {
    $taskOutput = ".pio/${Name}_host.exe"
    & $Compiler '-std=c++14' '-Wall' '-Wextra' @Extra '-I' 'include' @Sources '-o' $taskOutput
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $Name" }
    & "./$taskOutput"
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $Name" }
  }
  foreach ($taskName in @('vario','compass','battery','russian','calendar','flight_log')) {
    Invoke-HostTest $taskName @("test/${taskName}_host.cpp")
  }
  Invoke-HostTest 'devices' @('test/devices_host.cpp','src/rtc_clock.cpp','src/power_monitor.cpp') @('-I','test/device_stub')
  Invoke-HostTest 'journal_storage' @('test/journal_storage_host.cpp','src/flight_journal.cpp') @('-I','test/journal_stub')
  $taskFlashOutput = '.pio/external_flash_host.exe'
  & $Compiler '-std=c++14' '-Wall' '-Wextra' '-I' 'test/external_stub' '-I' 'include' 'test/external_flash_host.cpp' 'src/external_log_flash.cpp' '-o' $taskFlashOutput
  if ($LASTEXITCODE -ne 0) { throw 'Compilation failed: external_flash' }
  foreach ($taskChip in @('macronix','winbond')) {
    & "./$taskFlashOutput" $taskChip
    if ($LASTEXITCODE -ne 0) { throw "Test failed: external_flash $taskChip" }
  }
  $taskLib = ".pio/libdeps/$Environment"
  if (-not (Test-Path "$taskLib/Bounce2/src/Bounce2.cpp")) {
    throw "Install dependencies first: pio pkg install -e $Environment"
  }
  Invoke-HostTest 'buttons' @('test/buttons_host.cpp',"$taskLib/Bounce2/src/Bounce2.cpp") @('-DARDUINO=10800','-I','test/arduino_stub','-I',"$taskLib/Bounce2/src")
  if ($Screens) {
    New-Item -ItemType Directory -Path .pio/ui -Force | Out-Null
    Invoke-HostTest 'screen' @('test/screen_host.cpp','src/diagnostic_display.cpp',"$taskLib/Adafruit GFX Library/Adafruit_GFX.cpp") @(
      '-DARDUINO=10800','-DVARIO_SCREEN_HOST','-I','test/screen_stub','-I','test/device_stub','-I',"$taskLib/Adafruit GFX Library")
  }
} finally { Pop-Location }
