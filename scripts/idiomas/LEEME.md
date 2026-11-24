# Idiomas de Satronho Emulation

Los textos se escriben en español en el código. `Tr()` (src/util/i18n.cpp) los cambia por su
traducción de `assets/idiomas/<idioma>.json` (en, fr, pt, it). Lo que no está traducido sale en español.

- `envolver_tr.py archivo.cpp…`: envuelve con `Tr()` el texto de las llamadas que dibujan (ImGui::Text…,
  MenuItem, AddText…). Las funciones del kit (`RowBegin`, `Button`, `Toast`, `Muted`, `Fmt`…) ya traducen solas.
- `extraer.py archivos…`: saca los literales del código a `claves_nuevas.json` (compararlo con `claves.json`).
- `traducciones/<idioma>_N.txt`: `número|traducción`, el número es la posición en `claves.json`.
  `extra.txt`: `español|inglés|francés|portugués|italiano` para claves sueltas.
- `armar.py` (desde la raíz del repo): arma los .json y revisa que los `%d`, `%s`… coincidan.
- Frases que en el código se pegan con `+` se traducen por trozos (trozos que empiezan o terminan con
  espacio o signo). Mejor usar `Fmt("Ranura %d", n)` para frases nuevas.
- Pendiente: el chat con los guías (src/sys/guide_chat.cpp) responde en español.
