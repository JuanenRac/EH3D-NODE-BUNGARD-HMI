<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-HMI banner" width="100%">
</p>

# 🖥️ EH3D-NODE-BUNGARD-HMI

<p align="center"><a href="README.md">🇺🇸 English</a> | 🇪🇸 <b>Español</b> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Una pantalla táctil de 10,1 pulgadas con un ESP32-P4 que controla por sí sola una máquina Bungard CCD: las pantallas y el flujo del programa de escritorio, en ocho idiomas, avisos y órdenes por voz, y una parada por hardware que no pasa por la pantalla.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - lo que se ejecuta hoy:** las reglas de la pantalla están escritas en C++17 y probadas en un ordenador: el fichero de ajustes con sus valores seguros por defecto, la compuerta que debe pasar cada movimiento y cada orden de husillo (una confirmación en la pantalla, el pestillo de la parada por hardware, el enlace armado), las órdenes y avisos por voz en ocho idiomas y los textos de las pantallas. El firmware de ESP-IDF con las pantallas LVGL está escrito para el módulo Guition JC8012P4A1C pero **no se ha compilado, grabado ni ejecutado en un módulo**, todavía no hay enlace con el servidor ni con la máquina y no se ha conectado nada a una máquina.

---

> **Estado de seguridad (safety) y de seguridad informática:** esto no es un sistema de seguridad (safety). La parada que protege a las personas y a la máquina es un pulsador por hardware que corta el accionamiento por sí mismo, cableado aparte de esta pantalla; la pantalla solo lo lee, lo muestra y se niega a mover nada mientras está pulsado o sin reconocer. Un movimiento o una orden de husillo pide una confirmación en la pantalla, una orden por voz la pide siempre, y las órdenes por voz están desactivadas hasta que el operador las activa. No uses esta pantalla para manejar una máquina hasta que toda la cadena se haya probado en una real. El proyecto es independiente: no está hecho, avalado ni respaldado por Bungard Elektronik ni por Guition, y los nombres de producto aparecen solo para decir con qué máquinas y módulos está pensado para funcionar.

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.0.1` |
| Madurez | Andamiaje |
| Pruebas | 1 programa de prueba en el ordenador (598 comprobaciones contadas) |
| Pila tecnológica | C++17, ESP-IDF 5.5, LVGL 9, ESP32-P4 |
| En hardware real | No: el firmware no se ha compilado ni grabado y nada se ha conectado a una máquina |

**Ir a:** [Resumen](#-resumen) · [Arquitectura](#-arquitectura) · [Inicio rápido](#-inicio-rápido) · [Pruebas](#-pruebas) · [Estructura](#-estructura-del-repositorio) · [Relacionados](#-proyectos-relacionados)

## 🚦 Estado del proyecto

Cada línea es una parte de este repositorio y la luz dice hasta dónde llega hoy. Se escribe con lo que el repositorio puede demostrar (pruebas, compilaciones, ejecuciones), no con lo previsto.

🟢 Hecho y verificado · 🟡 Funciona, con límites conocidos · 🟠 Hecho a medias o sin verificar · 🔴 Sin hacer o sin funcionar

| Área | Estado | Qué significa aquí |
|---|---|---|
| Software | 🟠 | El proyecto ESP-IDF (ESP32-P4, LVGL 9, el fichero de ajustes en su propia partición) está escrito; no se ha compilado ni grabado. |
| Pruebas | 🟢 | Pasa 1 programa de prueba en el ordenador (`ctest`): 598 comprobaciones del fichero de ajustes, la compuerta, las órdenes y avisos por voz y los textos de las pantallas en ocho idiomas. |
| Reglas de seguridad | 🟡 | Todo movimiento y toda orden de husillo pasan por la compuerta: confirmación en la pantalla, pestillo de la parada por hardware que solo borra un reconocimiento en la pantalla cuando el pulsador está suelto, ningún movimiento con el enlace sin armar. Cubierto por pruebas; nunca probado con un pulsador de parada real. |
| Voz | 🟡 | Las órdenes y avisos por voz se reconocen y se formulan en ocho idiomas y se prueban como texto; no se ha usado ningún micrófono ni altavoz. |
| Pantallas | 🟠 | La pantalla de la máquina (movimiento manual, origen, husillo, inicio, parada, confirmación, aviso de parada por hardware) está escrita; las pestañas de visor, trabajo, máquinas y estaciones y ajustes siguen siendo páginas vacías. |
| Hardware real | 🔴 | Ningún módulo ha ejecutado este firmware, el enlace con el servidor todavía no existe y no se ha conectado nada a una máquina. |
| Documentación | 🟢 | README en ocho idiomas, la referencia de ajustes y las notas de hardware. |

## 🎯 Resumen

* La **compuerta**: todo movimiento y toda orden de husillo, venga de un toque o de la voz, pasa por un único lugar que decide ejecutarlo, pedir una confirmación o rechazarlo con un motivo; una parada siempre pasa.
* El **fichero de ajustes** `hmi.cfg`: todo valor afinado (idioma, brillo, el servidor, los interruptores de seguridad, la voz) es una línea de un fichero en su propia partición, nunca un número en el código; un valor fuera de rango se avisa y se queda el valor seguro por defecto.
* La **voz**: avisos hablados (trabajo terminado, cambio de herramienta, error, enlace perdido, parada por hardware) y una lista corta y fija de órdenes por voz en ocho idiomas; las órdenes que solo detienen se entienden dentro de una frase, las que mueven o giran deben decirse enteras y piden siempre una confirmación.
* Las **pantallas**: la máquina (movimiento manual, origen, husillo, inicio, parada), el trabajo, el visor, las máquinas y estaciones del servidor y los ajustes, en el orden y con los nombres del programa de escritorio; el botón de parada está siempre arriba.
* El **módulo**: el Guition JC8012P4A1C (ESP32-P4 con 32 MB de RAM externa, pantalla táctil MIPI-DSI de 10,1 pulgadas y 800x1280, un ESP32-C6 para el Wi-Fi, el códec ES8311 con amplificador de altavoz y la entrada de micrófono ES7210). El panel de la unidad es el nuevo.

## 🧩 Papel en EH3D

EH3D-NODE-BUNGARD-HMI es la pantalla propia del operador de una máquina Bungard CCD: ocupa el lugar del PC junto a la máquina. Está pensada para manejar la máquina a través de EH3D-SERVER o de EH3D-NODE-BUNGARD (el nodo del puerto serie de la máquina), igual que EH3D-DRILLPRO y EH3D-ROUTEPRO. Ese enlace todavía no está escrito.

## 🏛️ Arquitectura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitectura de EH3D-NODE-BUNGARD-HMI: quién lo usa, qué contiene y de qué depende" width="100%">
</p>

Cómo encajan las partes de este repositorio: quién habla con él, qué contiene y de qué depende. El dibujo sale de `docs/architecture.json` y se comprueba contra el código.

## ✨ Funciones

* **Una parada que no se puede olvidar:** el botón de parada está siempre arriba y siempre funciona; la parada por hardware se enclava y solo un reconocimiento en la pantalla, con el pulsador suelto, la borra.
* **Confirmación antes de mover:** un movimiento o una orden de husillo muestra lo que hará y espera un toque, con tiempo límite; una petición nueva o un enlace perdido la descarta.
* **Ocho idiomas, también por voz:** inglés, español, italiano, alemán, francés, portugués de Portugal, chino y japonés en las pantallas, en los avisos hablados y en las órdenes por voz.
* **Seguro por defecto:** cada interruptor de seguridad tiene su ajuste y empieza en su posición conservadora: órdenes por voz desactivadas, confirmación por voz desactivada, confirmación activada, parada por hardware exigida.

## 🔒 Seguridad y protección

| Amenaza | Defensa |
|---|---|
| Un toque o una orden por voz que mueve la máquina por accidente | un movimiento pide una confirmación en la pantalla con tiempo límite; una orden por voz siempre la pide; un sí hablado confirma solo con el ajuste `voice_confirm` activado (desactivado por defecto) |
| La pantalla empieza a mover con la parada por hardware pulsada | la parada por hardware es un pestillo: pulsada, o suelta pero sin reconocer en la pantalla, se rechaza todo movimiento y toda orden de husillo y se descarta lo que esperaba; una parada pulsada mientras espera una confirmación la cancela |
| Una máquina que no está lista | nada se mueve con el enlace sin armar, con la entrada de parada por hardware sin cablear (`hw_stop_required`, activado por defecto) o con el límite de husillo en 0 |
| Una palabra hablada tomada por una orden | las órdenes salen de una lista corta y fija por idioma, nunca de texto libre; una palabra que solo contiene una orden (`stopwatch`) no es una orden; las órdenes por voz están desactivadas hasta que el operador las activa |
| Un error de escritura en los ajustes | un valor fuera de rango o una clave desconocida se avisa y se queda el valor seguro por defecto; ningún interruptor se abre por un error |
| Un secreto en el firmware o en el repositorio | no se compila nada secreto; el token es un ajuste en su partición y nunca se muestra ni se registra |

**Lo que no se defiende:** la pantalla no es un dispositivo de seguridad y no puede cortar el accionamiento de la máquina; eso debe hacerlo la parada por hardware por sí sola. Todavía no hay enlace con el servidor, así que hoy la pantalla no puede mover ninguna máquina. El reconocimiento de voz se prueba como texto; el camino real del micrófono (cancelación de eco, ruido, acentos) no se ha probado. Quien tenga el módulo en las manos puede cambiar el fichero de ajustes.

## 🔌 Interfaces

| Interfaz | Qué es | Dónde |
|---|---|---|
| Fichero de ajustes | `hmi.cfg` en la partición `config`: líneas `clave = valor`; ver `hmi.cfg.example` y la tabla anterior | `core/hmi_config.hpp` |
| Entrada de parada por hardware | un GPIO (`HMI_HW_STOP_GPIO` en menuconfig, activo en bajo por defecto, con pull-up para que un cable abierto cuente como pulsado) | `main/board_p4.cpp` |
| Pantalla táctil | MIPI-DSI de 800x1280 mediante LVGL 9 | `main/ui.cpp` |
| Sonido | códec ES8311 con amplificador de altavoz y la entrada de micrófono ES7210 | `main/board_p4.cpp` |

## ⚙️ Configuración

Todo lo afinado es una línea de `hmi.cfg` (copia `hmi.cfg.example` a la partición `config`); un valor fuera de rango se avisa en el registro y se queda el valor por defecto. Los valores de abajo son los de por defecto; todos los de seguridad son los conservadores.

| Ajuste | Por defecto | Rango | Qué hace |
|---|---|---|---|
| `language` | `en` | `en es it de fr pt zh ja` | el idioma de las pantallas y de la voz |
| `confirm_moves` | on | on / off | pide confirmación para mover: desactívalo solo en el banco de pruebas |
| `confirm_timeout_ms` | 8000 | 1000 - 60000 | cuánto espera una confirmación |
| `jog_max_mm` | 20 | 1 - 200 | el movimiento manual más largo que envía la pantalla |
| `spindle_max_rpm` | 24000 | 0 - 65535 | la velocidad máxima de husillo que pide (0: nunca arranca el husillo) |
| `hw_stop_required` | on | on / off | maneja la máquina solo con la entrada de parada por hardware cableada y suelta |
| `voice_orders` | off | on / off | acepta órdenes por voz |
| `voice_confirm` | off | on / off | acepta un sí hablado como confirmación de un movimiento |
| `token` | none | 8 - 64 characters | el token de acceso al servidor; solo escritura, nunca se muestra ni se registra |

## 🚀 Inicio rápido

Necesitas un compilador de C++17 y CMake para las pruebas en el ordenador, y ESP-IDF 5.5.5 para el firmware (el objetivo ESP32-P4 del módulo).

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
python -I tools/ci_validate.py
```

El firmware todavía no se ha compilado: cuando se compile, `idf.py set-target esp32p4 && idf.py build` genera la imagen y una partición llamada `config` guarda `hmi.cfg`.

## 🧪 Pruebas

Las pruebas en el ordenador (`build.bat` o `./build.sh`, y luego `ctest`) son un programa, `test_hmi`, y pasa entero: 598 comprobaciones del fichero de ajustes (valores por defecto, rangos, errores, finales de línea), la compuerta (la parada siempre pasa, pestillo de la parada por hardware, confirmación, tiempo límite y vuelta del reloj, reglas de voz, interruptores), las órdenes y avisos por voz en ocho idiomas y los textos de las pantallas.

## 🏗️ Compilación y publicación

La versión vive en `eh3d.project.json` y cada versión tiene su entrada en [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilan y ejecutan las pruebas en el ordenador y `test.sh` / `test.bat` ejecutan las comprobaciones del repositorio. El firmware se compila fuera de los scripts de este repositorio, en una ventana visible, de una en una.

## 🩺 Resolución de problemas

| Síntoma | Qué hacer |
|---|---|
| No se mueve nada y la pantalla dice que la parada por hardware está pulsada o sin reconocer | Suelta el pulsador y pulsa el botón de reconocer de la pantalla: una parada que se soltó no se borra sola. |
| La pantalla dice que la máquina no está armada | En esta versión todavía no hay enlace con la máquina; la compuerta rechaza todo movimiento hasta que un enlace la arme. |
| La pantalla dice que la entrada de parada por hardware no está cableada | Cablea el pulsador de parada al pin fijado en menuconfig (`HMI_HW_STOP_GPIO`), o desactiva `hw_stop_required` solo en el banco. |
| Una orden por voz no hace nada | Las órdenes por voz están desactivadas por defecto: pon `voice_orders = on`. Las que mueven o giran piden un toque en la pantalla salvo que `voice_confirm` esté activado. |

## 📂 Estructura del repositorio

<p align="center">
  <img src="images/repository-structure.svg" alt="Estructura del repositorio EH3D-NODE-BUNGARD-HMI" width="100%">
</p>

<details>
<summary>Versión en texto del árbol</summary>

```text
EH3D-NODE-BUNGARD-HMI/
├── core/
│   ├── hmi_config.hpp lang.hpp                  el fichero de ajustes y sus valores seguros
│   ├── gate.hpp                                 la compuerta: confirmación, pestillo de la parada
│   ├── voice.hpp                                órdenes y avisos por voz en ocho idiomas
│   └── ui_text.hpp                              los textos de las pantallas en ocho idiomas
├── main/
│   ├── app_main.cpp                             arranque, ajustes, el bucle de la compuerta
│   ├── ui.cpp ui.hpp                            las pantallas LVGL y el botón de parada
│   ├── board_p4.cpp board.hpp                   el módulo: pantalla, sonido, entrada de parada
│   └── idf_component.yml Kconfig.projbuild      lista de componentes y pines de menuconfig
├── tests/                                       pruebas en el ordenador de los ajustes, la compuerta, la voz y los textos de pantalla
├── docs/                                        las notas de hardware y las fuentes de los dos dibujos
├── tools/                                       comprobación de CI, de README y herramientas de dibujos
├── CMakeLists.txt                               el proyecto ESP-IDF
├── partitions.csv                               el reparto de la flash con dos ranuras
├── hmi.cfg.example                              cada ajuste con su valor por defecto y su rango
├── sdkconfig.defaults                           ajustes de ESP-IDF para el módulo ESP32-P4
├── images/                                      banner y las imágenes de Arquitectura y Estructura del repositorio
├── .github/                                     flujos de CI y de publicación, plantillas de issues y pull requests
├── build.sh build.bat                           compila y ejecuta las pruebas en el ordenador con un panel en vivo
├── eh3d.project.json                            manifiesto: versión, rol, madurez y comando de compilación
├── CHANGELOG.md                                 qué cambió en cada versión
├── SECURITY.md SUPPORT.md                       cómo informar de una vulnerabilidad y dónde preguntar
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   cómo contribuir, el código de conducta y la licencia
└── bump_manifest_version.py                     utilidad de versión del manifiesto
```

</details>

## 🗺️ Hoja de ruta y límites conocidos

**Límites conocidos hoy**

* Nada se ha ejecutado en un módulo: el firmware no se ha compilado, grabado ni probado, y el paquete de soporte de placa que pide puede necesitar cambios para este módulo.
* Todavía no hay enlace con EH3D-SERVER ni con el nodo de la máquina, así que las pantallas no pueden mover una máquina; las pestañas de visor, trabajo, máquinas y estaciones y ajustes son páginas vacías.
* El pin de la parada por hardware y el controlador táctil todavía no están fijados: hay que comprobarlos con el esquema de la unidad.
* El reconocedor de voz y el altavoz todavía no están en el firmware; las órdenes y avisos se prueban como texto.
* La lógica de los programas de escritorio (formatos, controlador, flota) no se reutiliza: las pantallas siguen al programa de escritorio, pero las reglas se reescriben en C++.

**Siguiente**

* Compilar el firmware y arrancar el módulo: pantalla, táctil y sonido.
* Escribir el enlace con EH3D-SERVER y con el nodo, y llenar las demás pestañas de las pantallas.
* Añadir el reconocedor de voz y el altavoz, y probarlos con cancelación de eco.
* Cablear un pulsador de parada real y probar el pestillo en una máquina.

## 🌍 Idiomas

El README y los textos existen en inglés, español, francés, italiano, alemán, chino, japonés y portugués de Portugal.

## 🔗 Proyectos relacionados

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): el programa de escritorio que siguen las pantallas
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): el programa de fresado; comparte el fichero de máquina y la biblioteca del protocolo
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): el centro de la familia: ejecuta los trabajos de todos los clientes
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): el nodo del puerto serie de la unidad de control de la máquina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): las partes comunes de la familia (formatos, controlador, flota, temas)

## 📚 Documentación

* Los ajustes: `hmi.cfg.example` lista cada clave con su valor por defecto y su rango.
* El hardware: los pines del módulo y las notas que acompañan a la demo del fabricante, en `docs/HARDWARE.md`.
* [Registro de cambios](CHANGELOG.md)

## 🤝 Contribuir y comunidad

* Antes de una pull request ejecuta las pruebas en el ordenador (`./build.sh` o `build.bat`) y `python -I tools/ci_validate.py`: todo debe pasar. Mira [CONTRIBUTING.md](CONTRIBUTING.md) y el [código de conducta](CODE_OF_CONDUCT.md).
* El README existe en ocho idiomas: un cambio del texto se hace en los ocho ficheros, y los dos dibujos salen de `docs/architecture.json` y `docs/structure.json` con `tools/make_architecture.py` y `tools/make_structure.py`.
* Nada que permita a la pantalla mover una máquina cambia sin una prueba de la compuerta que lo cubra y una nota de lo observado en hardware.
* Informa de una vulnerabilidad en privado: [SECURITY.md](SECURITY.md). Preguntas, ideas e informes: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

Software: GPL-3.0-or-later. Véase [LICENSE](LICENSE). Los diseños de hardware de la suite usan CERN OHL-S.
