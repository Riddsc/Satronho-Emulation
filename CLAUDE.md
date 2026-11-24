# Satronho Emulation — instrucciones para Claude

Antes de tocar nada lee, en este orden:

1. `CONTINUAR.md` — dónde quedó el proyecto, qué falta probar y qué sigue.
2. `SATRONHO.md` — cómo está hecho (memoria técnica).

## Cómo trabaja Ridds (dueño del proyecto)

- Todo en **español**: respuestas, textos de la app, comentarios del código, commits y notas.
- **Commits siempre a nombre de Riddsc <richardsc921@gmail.com>**, nunca como Claude y sin líneas
  `Co-Authored-By`/`Claude-Session`. Antes de commitear:
  `git config user.name "Riddsc" && git config user.email "richardsc921@gmail.com"`.
- Trabaja en **modo manual**: si junto con una tarea hace preguntas o pide explicaciones, primero
  se responde y se espera su visto bueno antes de cambiar código.
- No usa artifacts: las builds van a **GitHub Releases** (`build-N`), las publica el CI solo al
  hacer push a `main`. Nada de subir artifacts de Actions.
- Es principiante en programación: explicar en palabras sencillas, sin jerga innecesaria.

## Reglas del proyecto

- Los textos de la app hablan con la voz de los guías (Samy, Troya, Ñoño, Juana, MechaRidds) y usan
  los nombres Satronho («Satronho Octa · para juegos de NES»), nunca logos ni marcas como nombre.
- Cada build nueva: agregar una entrada al principio de `assets/novedades.json` con
  `"version": "Esta build"` y pasar la anterior a `"Build N"` (la primera entrada son las notas del
  Release).
- Antes de publicar: compilar (Linux nativo y, si se puede, Windows con llvm-mingw) y pasar el modo
  de revisión (`SATRONHO_QA`, ver `SATRONHO.md`). Mirar las capturas.
- Al terminar una sesión con cambios, actualizar `CONTINUAR.md` (estado, pendientes, ideas).
