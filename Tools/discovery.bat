@echo off
:: Enable delayed expansion
setlocal EnableDelayedExpansion

set stm32programmercli="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

@echo on
%stm32programmercli% -c port=SWD debugauth=2
@echo off
if !errorlevel! neq 0 goto :error

echo "discovery script success"
IF [%1] NEQ [AUTO] cmd /k
exit 0

:error
echo "discovery script failed"
IF [%1] NEQ [AUTO] cmd /k
exit 1
