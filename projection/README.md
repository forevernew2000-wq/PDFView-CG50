# ProyCalc fx-CG50

Calculadora para las dos hojas del Excel del curso:

- Proyeccion Plana Equidistante Meridiana
  - delta = 90 - phi
  - m = R * delta (radianes)
- Proyeccion Plana Equidistante Transversal
  - delta = 90 - phi
  - m = R * sin(delta)

En ambas:
- Y = m * sin(lambda)
- X = m * cos(lambda)
- coordenadas del plano en cm = metros / escala * 100

## Malla 3x3

La app recibe tres latitudes (phi1..phi3) y tres longitudes (lambda1..lambda3) y calcula:
A, D, G, B, E, H, C, F, I, igual que el Excel de referencia.

Valores iniciales:
- R = 6378137
- escala = 1:400000
- phi = 10°00', 10°10', 10°20'
- lambda = 50°30', 50°20', 50°10'

## Controles

- F1/F2 en el menu: escoger proyeccion
- Flechas: seleccionar
- EXE: editar / ver detalle
- F1 en datos: calcular
- F6: ver formulas
- F1 en resultados: alternar cm/metros
- F2 en resultados: resumen max/min/dimensiones
- EXIT: volver
