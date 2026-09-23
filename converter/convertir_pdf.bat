@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" (
  echo Arrastra un archivo PDF encima de convertir_pdf.bat
  echo.
  echo IMPORTANTE: el conversor procesa TODAS las paginas que existan en el PDF.
  echo Si al final dice 1/1, entonces el PDF de origen solo tiene una pagina.
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
echo Convirtiendo TODAS las paginas con detalle alto...
echo.
py pdf_to_cgv.py "%~1" -o "%~dp0PDFVIEW.CGV" --max-side 1024
if errorlevel 1 (
  echo.
  echo Hubo un error durante la conversion.
  pause
  exit /b 1
)

echo.
echo Listo. Copia PDFVIEW.CGV a la raiz de la fx-CG50.
echo Usa F1/F2 para cambiar de pagina.
pause
