@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" (
  echo Arrastra uno o VARIOS archivos PDF encima de convertir_pdf.bat
  echo.
  echo Se creara un archivo .CGV por cada PDF usando el mismo nombre.
  echo Ejemplo: Cartografia.pdf -^> Cartografia.CGV
  echo.
  pause
  exit /b 1
)

py -c "import fitz, PIL" >nul 2>&1
if errorlevel 1 (
  echo Instalando dependencias de Python...
  py -m pip install -r requirements.txt
  if errorlevel 1 (
    echo No se pudieron instalar las dependencias.
    pause
    exit /b 1
  )
)

echo.
echo Convirtiendo TODOS los PDFs y TODAS sus paginas...
echo.

:loop
if "%~1"=="" goto done

echo ========================================
echo PDF: %~nx1
echo Salida: %~n1.CGV
echo ========================================
py pdf_to_cgv.py "%~1" -o "%~dp0%~n1.CGV" --max-side 1024
if errorlevel 1 (
  echo.
  echo ERROR convirtiendo: %~nx1
  echo.
) else (
  echo OK: %~n1.CGV
  echo.
)

shift
goto loop

:done
echo ========================================
echo LISTO
echo ========================================
echo.
echo Copia a la raiz de la fx-CG50 todos los .CGV que quieras usar.
echo PDFView v3 los mostrara en una lista para elegir.
echo.
pause
