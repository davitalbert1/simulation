@echo off
setlocal EnableExtensions

rem  build_all.bat - compila todos os projetos via CMake
rem  Uso: build_all.bat [clean|rebuild]

set "BUILD_DIR=build"

cd /d "%~dp0"

if /I "%~1"=="clean" goto :clean_all
if /I "%~1"=="rebuild" goto :rebuild_all
if "%~1"=="" goto :build_all

:usage
echo.
echo Opcao invalida: %~1
echo Uso: build_all.bat [clean^|rebuild]
echo.
echo   (sem argumento) - Compilar todos os projetos com CMake
echo   clean           - Remover os executaveis e as pastas de build
echo   rebuild         - Limpar e compilar tudo
exit /b 1

rem BUILD
:build_all
echo.
echo ========================================
echo  BUILDING ALL PROJECTS (CMake)
echo ========================================

call :setup_tools
if errorlevel 1 exit /b 1
call :require_tools
if errorlevel 1 exit /b 1

set /a built=0
set /a failed=0

rem O for /r gera uma entrada por pasta da arvore; o "if exist" garante
rem que apenas pastas com CMakeLists.txt (ou seja, projetos) sejam processadas
for /r %%F in (CMakeLists.txt) do (
    if exist "%%F" (
        echo %%F | findstr /i /c:".git\" /c:"\build\" >nul
        if errorlevel 1 (
            echo.
            echo ----------------------------------------
            echo Projeto: %%~dpF
            echo ----------------------------------------
            pushd "%%~dpF"
            call :cmake_build
            if errorlevel 1 (
                set /a failed+=1
            ) else (
                set /a built+=1
            )
            popd
        )
    )
)

echo.
echo ========================================
echo  RESULTADO
echo ========================================
echo Projetos compilados: %built%
echo Projetos com erro:   %failed%
echo.

if not "%failed%"=="0" (
    echo Alguns projetos falharam. Verifique as mensagens acima.
    exit /b 1
)
echo Todos os builds finalizados.
exit /b 0

rem CLEAN
:clean_all
echo.
echo ========================================
echo  CLEANING ALL EXECUTABLES
echo ========================================

call :setup_tools

set /a cleaned=0

for /r %%F in (CMakeLists.txt) do (
    if exist "%%F" (
        echo %%F | findstr /i /c:".git\" /c:"\build\" >nul
        if errorlevel 1 (
            pushd "%%~dpF"
            call :cmake_clean
            popd
            set /a cleaned+=1
        )
    )
)

echo.
echo Limpeza finalizada! (%cleaned% projetos verificados)
exit /b 0

:rebuild_all
call :clean_all
if errorlevel 1 exit /b 1
goto :build_all

rem FERRAMENTAS / CMAKE
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

rem CMAKE (dentro da pasta do projeto)
:cmake_build
if exist "%BUILD_DIR%\CMakeCache.txt" goto :cmake_build_cached
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" %GEN_ARGS% -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
goto :cmake_build_run

:cmake_build_cached
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

:cmake_build_run
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --parallel
exit /b %errorlevel%

:cmake_clean
echo Limpando %CD%
if defined CMAKE_EXE if exist "%BUILD_DIR%\CMakeCache.txt" "%CMAKE_EXE%" --build "%BUILD_DIR%" --target clean
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist *.exe del /f /q *.exe
exit /b 0
