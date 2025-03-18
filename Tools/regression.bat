@echo off
:: Enable delayed expansion
setlocal EnableDelayedExpansion

set stm32programmercli="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

:: Select Key/certificate (if TZEN enabled) and password (if TZEN disabled)
set key=key_1_root.pem
set cert=cert_root.b64

@echo on
%stm32programmercli% -c port=SWD per=a key=%key% cert=%cert% debugauth=1
@echo off
if !errorlevel! neq 0 goto :error

echo "regression script success"
IF [%1] NEQ [AUTO] cmd /k
exit 0

:error
echo "regression script failed : run ./discovery.bat script to check if the product state is not already in open state."
IF [%1] NEQ [AUTO] cmd /k
exit 1
