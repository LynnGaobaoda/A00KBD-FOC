@echo off
rem Copy Keil axf/bin/hex into D:\A00KBD\FOC\out
rem %1 = Objects directory, %2 = basename (motor or boot)
setlocal
set "OBJ=%~1"
set "NAME=%~2"
set "OUT=D:\A00KBD\FOC\out"
set "FROMELF=D:\mysorftware\Keil_mdk\ARM\ARMCC\bin\fromelf.exe"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%\%NAME%.axf" (
  echo missing %OBJ%\%NAME%.axf
  exit /b 1
)
"%FROMELF%" --bin --output="%OUT%\%NAME%.bin" "%OBJ%\%NAME%.axf"
copy /Y "%OBJ%\%NAME%.axf" "%OUT%\%NAME%.axf" >nul
if exist "%OBJ%\%NAME%.hex" copy /Y "%OBJ%\%NAME%.hex" "%OUT%\%NAME%.hex" >nul
echo published %OUT%\%NAME%.axf %OUT%\%NAME%.bin
exit /b 0
