@echo off
setlocal

for /r %%F in (build.bat) do (
    echo %%F | findstr /i "\.git" >nul
    if errorlevel 1 (
        if exist "%%F" (
            echo.
            echo Executando %%F

            pushd "%%~dpF"
            call "%%~nxF"
            popd
        )
    )
)

echo.
echo Todos os builds finalizados.
exit