@echo off
setlocal
set ROOT_DIR=%~dp0..

for /d %%D in ("%ROOT_DIR%\build*") do (
    echo Removing %%D
    rmdir /s /q "%%D"
)

endlocal
exit /b 0
