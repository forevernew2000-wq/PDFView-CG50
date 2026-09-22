# PDFView-CG50

Visor experimental de documentos para la Casio fx-CG50.

La calculadora no interpreta PDF directamente. El PDF se convierte en PC a `PDFVIEW.CGV`; el add-in `PDFView.g3a` abre ese archivo desde la raíz del almacenamiento.

## Controles

- `F1` / `F2`: página anterior / siguiente
- `+`: vista ampliada
- `-`: página completa
- Flechas: desplazamiento al ampliar
- `F6`: ayuda
- `EXIT`: salir

## Instalación en la calculadora

Copia ambos archivos a la raíz de la memoria de almacenamiento:

- `PDFView.g3a`
- `PDFVIEW.CGV`

Luego expulsa la calculadora de forma segura y abre PDFView desde MENU.

## Compilación

El repositorio incluye un workflow de GitHub Actions que instala fxSDK + gint, compila con:

```bash
fxsdk build-cg
```

y publica `PDFView.g3a` como artifact.

El proyecto usa fxSDK/gint para fx-CG50.
