# ABDMS2000 — Virtual Korg MS2000 & microKORG (con Modo Avanzado)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![JUCE 8](https://img.shields.io/badge/JUCE-8.0-orange.svg)](https://juce.com/)
[![WebUI](https://img.shields.io/badge/UI-WebView2%20%2F%20HTML%2FCSS%2FJS-brightgreen.svg)]()
[![WASM](https://img.shields.io/badge/WASM-AudioWorklet-purple.svg)]()
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

**ABDMS2000** es un sintetizador virtual analógico híbrido y editor/gestor integral basado en la arquitectura de síntesis del **Korg MS2000** y **microKORG**, incorporando además un **Modo Avanzado (ABD Ultra Engine)**.

---

## 🌟 Características Principales

1. **Arquitectura Tri-Modo conmutable:**
   - **Modo Korg MS2000:** Réplica fiel con panel analógico azul, potenciómetros dedicados, pantalla LCD de 2 líneas (menús oficiales 1A-4D) y Secuenciador de Modulación de 3 pistas × 16 pasos.
   - **Modo microKORG:** Panel retro con matriz gráfica, diales de género musical, perillas de edición dinámica y display LED de 3 dígitos.
   - **Modo Avanzado (ABD Ultra):** Polifonía de hasta 32 voces, Reverb Shimmer, Tape Delay analógico, 4 LFOs, 4 Envolventes DAHDSR y Secuenciador polifónico de 64 pasos.
2. **Motor de Síntesis Fiel al Hardware:**
   - 2 Osciladores (68 ondas en OSC 1 incluyendo las 64 tablas DWGS + 3 en OSC 2 + Noise + Audio In).
   - Ring Modulation, Hard Sync y Cross Mod (FM).
   - Filtro Multimodo (LP24, LP12, BP12, HP12) con modelado no lineal y compensación de ganancia analógica.
   - Distorsión asimétrica post-filtro dependiente del nivel de mezcla.
   - Vocoder de 16 bandas con Formant Shift y HPF Gate (detector de transitorios para consonantes).
3. **Telemetría y Control MIDI / SysEx Bidireccional:**
   - Control en tiempo real del hardware físico vía CCs y NRPNs.
   - Petición y recepción de volcados (*Program Dump* y *All Data Dump*) mediante códec Korg 7-to-8 bits.
   - Pantalla dedicada **SysEx Inspector** con visor/editor Hexadecimal y copiado/pegado.
4. **Gestor Universal de Bancos (ABD Universal Bank Manager):**
   - Sistema de adaptadores de modelo (`ISynthModelContract`) y tokens de diseño CSS.
   - Almacenamiento infinito, búsqueda instantánea, categorización, tags y comparador visual A/B (Diff Viewer).
5. **Teclado Virtual Interactivo (Estilo ABDEep):**
   - Ruedas físicas de Pitch Bend y Mod Wheel.
   - Indicadores LED de transposición de octavas (Apagado / Parpadeo / Fijo).
   - Barra de luces LED con animaciones de cambio de patch (*Sweep Wave*), cambio de banco (*Ripple Pulse*) y pánico (*Flash Cut*).
6. **Multiplataforma:**
   - Standalone y VST3 para Windows (WebView2) y macOS (WKWebView).
   - Versión Web PWA compilada a WebAssembly (AudioWorklet) con IndexedDB.
   - Modo Headless / Servidor RPC para Raspberry Pi.

---

## 📁 Estructura del Proyecto

```
ABDMS2000/
├── CMakeLists.txt              # Configuración principal de build (JUCE 8)
├── build.bat                   # Script de compilación Standalone + VST3
├── ROADMAP.md                  # Seguimiento y validación de fases 0 a 9
├── implementation_plan.md      # Especificación técnica exhaustiva
├── DOCS/                       # Archivos de investigación, ingeniería inversa y manuales
├── schemas/                    # Contratos JSON canónicos
├── Scripts/                    # Generadores de código y empaquetadores
├── Source/                     # Código fuente C++ (Core, DSP, Plugin, MIDI, State)
├── WebUI/                      # Interfaz de usuario (HTML5, CSS3, ES Modules)
└── wasm/                       # Configuración de compilación Emscripten
```

---

## 🛠️ Compilación

### Requisitos
- **Windows:** Visual Studio 2022 (MSVC C++20) o Clang + CMake 3.22+
- **Node.js:** v18+ (para generadores y tests de UI)
- **Emscripten SDK:** (opcional, para compilar la versión WASM)

### Construcción
```bash
# Compilación completa (Genera Standalone y VST3 en build/)
build.bat
```

---

*Desarrollado por ajabadia — Suite de Sintetizadores ABD.*
