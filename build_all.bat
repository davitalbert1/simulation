@echo off
setlocal

if "%~1"=="clean" goto :clean_all
if "%~1"=="rebuild" goto :rebuild_all
if "%~1"=="" goto :build_all

echo.
echo Opcao invalida: %~1
echo Uso: build_all.bat [clean|rebuild]
echo.
echo   (sem argumento) - Compilar todos os projetos
echo   clean           - Deletar todos os executaveis
echo   rebuild         - Limpar e compilar tudo
exit /b 1

:build_all
echo.
echo ========================================
echo  BUILDING ALL PROJECTS
echo ========================================

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
exit /b 0

:clean_all
echo.
echo ========================================
echo  CLEANING ALL EXECUTABLES
echo ========================================

set "cleaned=0"
for /r %%F in (build.bat) do (
    echo %%F | findstr /i "\.git" >nul
    if errorlevel 1 (
        if exist "%%F" (
            pushd "%%~dpF"
            call "%%~nxF" clean
            popd
            set /a cleaned+=1
        )
    )
)

echo.
echo Limpeza finalizada! (%cleaned% projetos verificados)
exit /b 0

:rebuild_all
echo.
echo ========================================
echo  REBUILD ALL PROJECTS
echo ========================================

set "built=0"
for /r %%F in (build.bat) do (
    echo %%F | findstr /i "\.git" >nul
    if errorlevel 1 (
        if exist "%%F" (
            echo.
            echo Executando %%F

            pushd "%%~dpF"
            call "%%~nxF" rebuild
            popd
            set /a built+=1
        )
    )
)

echo.
echo Rebuild finalizado! (%built% projetos compilados)
exit /b 0