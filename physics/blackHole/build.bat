@echo off
setlocal

set "EXE_NAME=blackhole.exe"

if exist "%EXE_NAME%" (
    powershell -Command "$exe='%EXE_NAME%'; $exeTime=(Get-Item $exe).LastWriteTime; if (Get-ChildItem -Path * -Include *.cpp,*.h | Where-Object { $_.LastWriteTime -gt $exeTime }) { exit 1 } else { exit 0 }"
    if %errorlevel% == 0 (
        echo.
        echo ====================================================
        echo O executavel %EXE_NAME% ja esta atualizado. Pulando.
        echo ====================================================
        exit /b 0
    )
)

echo.
echo =====================================
echo Compilando %EXE_NAME%...
echo =====================================

g++ main.cpp window.cpp camera.cpp render.cpp ^
-o %EXE_NAME% ^
-lopengl32 -lgdi32 -lglu32

if %errorlevel% == 0 (
    echo.
    echo Concluido com sucesso!
) else (
    echo.
    echo Concluido com erro.
)

exit /b %errorlevel%