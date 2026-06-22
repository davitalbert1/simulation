@echo off
setlocal

set "EXE_NAME=visual_ordering.exe"

rem Verifica se o executavel ja existe
if exist "%EXE_NAME%" (
    rem Compara datas usando PowerShell para ver se algum arquivo fonte e mais novo que o executavel
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

g++ main.cpp window.cpp ^
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
