@echo off
setlocal

:: 定义源代码路径和目标位置
set "source=D:\A00KBD\FOC\FOCSimulinkFloat\FocCtrl_autosar_rtw"
set "destination=D:\A00KBD\FOC\FOC_Code\user\SWC_FOC"

:: 检查目标文件夹是否存在，不存在则创建
if not exist "%destination%" (
    mkdir "%destination%"
)

:: 复制 .c 和 .h 文件（包含子目录）
xcopy "%source%\*.c" "%destination%"  /Y
xcopy "%source%\*.h" "%destination%"  /Y

:: 输出完成信息
echo C 代码文件已成功拷贝到目标位置！
