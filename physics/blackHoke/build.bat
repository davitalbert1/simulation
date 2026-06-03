@echo off
echo Compilando black hole simulator...

g++ main.cpp window.cpp camera.cpp render.cpp ^
-o blackhole.exe ^
-lopengl32 -lgdi32 -lglu32

if %errorlevel% == 0 (
    echo.
    echo Concluido com sucesso!
) else (
    echo.
    echo Concluido com erro.
)

exit /b %errorlevel%
