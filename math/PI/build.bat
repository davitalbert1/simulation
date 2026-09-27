@echo off
setlocal EnableExtensions

rem ------------------------------------------------------------------
rem  Build do pi_simulation.exe via CMake
rem  Uso: build.bat [clean|rebuild]
rem ------------------------------------------------------------------

set "EXE_NAME=pi_simulation.exe"
set "BUILD_DIR=build"

cd /d "%~dp0"

if /I "%~1"=="clean" goto :clean
if /I "%~1"=="rebuild" goto :rebuild
if not "%~1"=="" (
    echo.
    echo Opcao invalida: %~1
    echo Uso: build.bat [clean^|rebuild]
    exit /b 1
)

rem ------------------------------- BUILD ----------------------------
:build
call :setup_tools
if errorlevel 1 exit /b 1
call :require_tools
if errorlevel 1 exit /b 1

echo.
echo Compilando %EXE_NAME% com CMake...

call :cmake_configure
if errorlevel 1 goto :error

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 goto :error

echo.
echo Concluido com sucesso!
echo Executavel: %EXE_NAME%
exit /b 0

:error
echo.
echo Concluido com erro.
exit /b 1

rem ------------------------------- CLEAN ----------------------------
:clean
if exist "%EXE_NAME%" (
    del /f /q "%EXE_NAME%"
    if exist "%EXE_NAME%" (
        echo.
        echo ERRO: nao foi possivel deletar %EXE_NAME% ^(esta em execucao^?^)
        exit /b 1
    )
    echo %EXE_NAME% deletado.
) else (
    echo %EXE_NAME% nao encontrado.
)

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo Pasta %BUILD_DIR% removida.
)
exit /b 0

:rebuild
call :clean
if errorlevel 1 exit /b 1
goto :build

rem ------------------- FERRAMENTAS / CMAKE --------------------------
:setup_tools
rem Localiza o MSYS2/MinGW e coloca g++, cmake e ninja no PATH
set "MSYS_BIN="
if exist "C:\msys64\mingw64\bin\g++.exe" set "MSYS_BIN=C:\msys64\mingw64\bin"
if not defined MSYS_BIN if exist "D:\msys64\mingw64\bin\g++.exe" set "MSYS_BIN=D:\msys64\mingw64\bin"
if not defined MSYS_BIN if exist "C:\msys64\mingw64\bin\cmake.exe" set "MSYS_BIN=C:\msys64\mingw64\bin"
if not defined MSYS_BIN if exist "D:\msys64\mingw64\bin\cmake.exe" set "MSYS_BIN=D:\msys64\mingw64\bin"
if defined MSYS_BIN set "PATH=%MSYS_BIN%;%PATH%"

set "CMAKE_EXE="
where cmake >nul 2>&1
if not errorlevel 1 set "CMAKE_EXE=cmake"
if not defined CMAKE_EXE if defined MSYS_BIN if exist "%MSYS_BIN%\cmake.exe" set "CMAKE_EXE=%MSYS_BIN%\cmake.exe"
if not defined CMAKE_EXE if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
if not defined CMAKE_EXE if exist "D:\Program Files\CMake\bin\cmake.exe" set "CMAKE_EXE=D:\Program Files\CMake\bin\cmake.exe"

rem Gerador do CMake: respeita CMAKE_GENERATOR caso o usuario defina
set "GEN_ARGS="
if defined CMAKE_GENERATOR exit /b 0
where ninja >nul 2>&1
if not errorlevel 1 set "GEN_ARGS=-G Ninja"
if not defined GEN_ARGS (
    where mingw32-make >nul 2>&1
    if not errorlevel 1 set "GEN_ARGS=-G "MinGW Makefiles""
)
if not defined GEN_ARGS (
    where make >nul 2>&1
    if not errorlevel 1 set "GEN_ARGS=-G "MinGW Makefiles""
)
exit /b 0

:require_tools
if not defined CMAKE_EXE (
    echo.
    echo ERRO: cmake nao encontrado.
    echo Instale o CMake ^(https://cmake.org/download/^) ou o MSYS2:
    echo   pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
    exit /b 1
)

where g++ >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERRO: compilador g++ nao encontrado no PATH.
    echo Instale o MSYS2 ^(https://www.msys2.org/^) e rode:
    echo   pacman -S mingw-w64-x86_64-gcc
    exit /b 1
)
exit /b 0

:cmake_configure
rem A opcao -G so e aplicada na primeira configuracao (o cache mantem o gerador)
if exist "%BUILD_DIR%\CMakeCache.txt" goto :cmake_reconfigure
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" %GEN_ARGS% -DCMAKE_BUILD_TYPE=Release
exit /b %errorlevel%

:cmake_reconfigure
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
exit /b %errorlevel%
