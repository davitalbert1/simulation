@echo off
setlocal

set "EXE_NAME=evolution_sim.exe"

rem Adiciona o caminho do compilador MinGW caso nao esteja no PATH
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    if exist "C:\msys64\mingw64\bin" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else if exist "D:\msys64\mingw64\bin" (
        set "PATH=D:\msys64\mingw64\bin;%PATH%"
    )
)

rem Verifica se o executavel ja existe e se precisa ser recompilado
if exist "%EXE_NAME%" (
    powershell -Command "$exe='%EXE_NAME%'; $exeTime=(Get-Item $exe).LastWriteTime; if (Get-ChildItem -Path * -Include *.cpp,*.h | Where-Object { $_.LastWriteTime -gt $exeTime }) { exit 1 } else { exit 0 }"
    if %errorlevel% == 0 (
        echo.
        echo O executavel %EXE_NAME% ja esta atualizado. Pulando.
        exit /b 0
    )
)

echo.
echo Compilando %EXE_NAME%...

g++ main.cpp window.cpp simulation.cpp render.cpp camera.cpp -o %EXE_NAME% -lopengl32 -lgdi32 -lglu32 -O2

if %errorlevel% == 0 (
    echo.
    echo Concluido com sucesso!
) else (
    echo.
    echo Concluido com erro.
)

exit /b %errorlevel%