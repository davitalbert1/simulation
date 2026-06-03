@echo off
echo Compilando...

g++ main.cpp window.cpp camera.cpp render.cpp ^
-o solar.exe ^
-lopengl32 -lgdi32 -lglu32

if %errorlevel% == 0 (
    echo.
    echo Concluido com sucesso!
) else (
    echo.
    echo Concluido com erro
)

exit