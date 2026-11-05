/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "ABDMS2000 — Korg MS2000 Hybrid Editor & DSP Emulator", "index.html", [
    [ "ABDMS2000 — Virtual Korg MS2000 &amp; microKORG (con Modo Avanzado)", "index.html", "index" ],
    [ "Mapa de Menús, Parámetros y Valores del Korg MS2000 (Árbol Completo)", "md_DOCS_2korg__ms2000__menu__map.html", [
      [ "PARTE 1: VOZ Y SINTETIZADOR", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md2", [
        [ "1. GLOBAL / VOICE (Configuración de la Arquitectura de Voz)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md3", null ],
        [ "2. PITCH (Afinación y Glide por Timbre)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md4", null ],
        [ "3. OSC 1 (Oscilador Principal)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md5", null ],
        [ "4. OSC 2 (Oscilador Secundario)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md6", null ],
        [ "5. MIXER (Mezclador de Señal)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md7", null ]
      ] ],
      [ "PARTE 2: FILTRO, AMPLIFICADOR Y ENVOLVENTES", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md9", [
        [ "6. VCF (Filtro Multimodo)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md10", null ],
        [ "7. VCA / AMP (Sección de Amplificación y Distorsión)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md11", null ],
        [ "8. EG1 &amp; EG2 (Generadores de Envolvente)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md12", null ],
        [ "9. LFO 1 &amp; LFO 2 (Osciladores de Baja Frecuencia)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md13", null ],
        [ "Virtual Patch (4 Ranuras de Modulación Matricial)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md14", null ]
      ] ],
      [ "PARTE 3: EFECTOS (FX), ECUALIZADOR (EQ) Y VOCODER", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md16", [
        [ "10. CHORUS/MOD FX (Efectos de Modulación)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md17", null ],
        [ "11. DELAY FX (Efectos de Retardo Maestro)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md18", null ],
        [ "12. EQUALIZER (EQ Paramétrico Global del Timbre)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md19", null ],
        [ "13. VOCODER (Filtros y Modos Especiales)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md20", null ]
      ] ],
      [ "PARTE 4: SECUENCIADORES Y AJUSTES GLOBALES", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md22", [
        [ "14. MOD SEQUENCE (Secuenciador de Modulación)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md23", null ],
        [ "15. GLOBAL SETTINGS (Ajustes de Sistema)", "md_DOCS_2korg__ms2000__menu__map.html#autotoc_md24", null ]
      ] ]
    ] ],
    [ "Investigación Completa del Korg MS2000 — Arquitectura, DSP e Ingeniería Inversa", "md_DOCS_2MS2000__Research__Notes.html", [
      [ "Índice", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md32", null ],
      [ "1. Visión General y Viabilidad", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md34", [
        [ "1.1 Qué información está disponible", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md35", null ],
        [ "1.2 Qué limitaciones existen", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md36", null ],
        [ "1.3 Estimación de fidelidad alcanzable", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md37", null ]
      ] ],
      [ "2. Recursos Disponibles en Internet", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md39", [
        [ "2.1 Mensajería SysEx, MIDI y Mapeo de Parámetros", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md40", null ],
        [ "2.2 Formas de Onda Digitales (DWGS)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md41", null ],
        [ "2.3 Circuitería y Estructura de Bloques", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md42", null ],
        [ "2.4 Datos de Audio de Referencia (Calibración)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md43", null ],
        [ "2.5 Reviews y Análisis Técnicos", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md44", null ]
      ] ],
      [ "3. Estrategia de Desarrollo: Editor Híbrido", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md46", [
        [ "3.1 El problema de los intentos anteriores", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md47", null ],
        [ "3.2 Flujo de datos del editor híbrido", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md48", null ],
        [ "3.3 Protocolo de calibración de curvas (sin osciloscopio)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md49", null ],
        [ "3.4 Arquitectura recomendada en JUCE", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md50", null ]
      ] ],
      [ "4. Arquitectura Completa del MS2000", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md52", [
        [ "4.1 Listado de Componentes (Módulos Internos por Voz)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md53", null ],
        [ "4.2 Componentes Globales (fuera de la voz)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md54", null ],
        [ "4.3 Interconexiones de Control Fijas (Ruteo por Defecto)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md55", null ]
      ] ],
      [ "5. Osciladores (71 formas de onda)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md57", [
        [ "5.1 Oscilador 1 (OSC1) — 68 formas de onda, 8 algoritmos", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md58", null ],
        [ "5.2 Oscilador 2 (OSC2) — 3 formas de onda", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md59", null ],
        [ "5.3 Algoritmos de Modulación de OSC2", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md60", null ],
        [ "5.4 Mixer", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md61", null ]
      ] ],
      [ "6. DWGS Wavetables (64 ondas digitales)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md63", [
        [ "6.1 Origen", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md64", null ],
        [ "6.2 Estrategia de Almacenamiento (La Gran Tabla)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md65", null ],
        [ "6.3 Código de Referencia", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md66", null ],
        [ "6.4 Integración con el Editor MIDI", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md67", null ]
      ] ],
      [ "7. Filtro Multimodo", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md69", [
        [ "7.1 Modos de Filtro y Datos SysEx", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md70", null ],
        [ "7.2 Conversión del Cutoff (Frecuencia de Corte)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md71", null ],
        [ "7.3 La Resonancia y la Pérdida de Graves (El Secreto)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md72", null ],
        [ "7.4 Modulación del Filtro (Intensity y Tracking)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md73", null ]
      ] ],
      [ "8. Envolventes (EG1 y EG2)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md75", [
        [ "8.1 Comportamiento de las Curvas", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md76", null ],
        [ "8.2 Tiempos Reales Medidos (Ingeniería Inversa)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md77", null ],
        [ "8.3 Fórmula de Mapeo SysEx → Tiempo Real", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md78", null ],
        [ "8.4 Coeficiente de Decay por Muestra", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md79", null ],
        [ "8.5 Origen de los datos (Documentado vs Deducido)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md80", null ]
      ] ],
      [ "9. LFOs (1 y 2)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md82", [
        [ "9.1 Formas de Onda", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md83", null ],
        [ "9.2 Comportamiento Libre (TEMPO_SYNC = OFF)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md84", null ],
        [ "9.3 Comportamiento Sincronizado (TEMPO_SYNC = ON)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md85", null ],
        [ "9.4 Origen de los datos", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md86", null ]
      ] ],
      [ "10. Distorsión (Amp Distortion)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md88", [
        [ "10.1 Comportamiento Oculto", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md89", null ],
        [ "10.2 Tipo de Distorsión", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md90", null ],
        [ "10.3 Implementación en JUCE", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md91", null ],
        [ "10.4 Posición en la Cadena de Audio", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md92", null ]
      ] ],
      [ "11. Secuenciador de Modulación (Mod Sequence)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md94", [
        [ "11.1 Arquitectura", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md95", null ],
        [ "11.2 Los Dos Modos de Transición", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md96", null ],
        [ "11.3 Datos SysEx (Documentado Oficialmente)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md97", null ],
        [ "11.4 Implementación del Motor", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md98", null ]
      ] ],
      [ "12. Arpegiador / Step Sequencer", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md100", [
        [ "12.1 Modos del Arpegiador", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md101", null ],
        [ "12.2 Parámetros SysEx", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md102", null ],
        [ "12.3 Implementación en JUCE (MidiBuffer)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md103", null ]
      ] ],
      [ "13. Virtual Patch (Matriz de Modulación)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md105", [
        [ "13.1 Orígenes de Modulación (Sources)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md106", null ],
        [ "13.2 Destinos de Modulación (Destinations)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md107", null ],
        [ "13.3 Intensidad", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md108", null ]
      ] ],
      [ "14. Efectos (EQ, Mod FX, Delay)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md110", [
        [ "14.1 Bloque 1: Ecualizador (EQ)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md111", null ],
        [ "14.2 Bloque 2: Efectos de Modulación (Mod FX)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md112", null ],
        [ "14.3 Bloque 3: Delay FX", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md113", null ]
      ] ],
      [ "15. Componentes Extra", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md115", [
        [ "15.1 Vocoder (16 bandas)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md116", null ],
        [ "15.2 Envelope Follower", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md117", null ],
        [ "15.3 Portamento / Glide", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md118", null ],
        [ "15.4 AMP Section (Amplificador + Panpot)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md119", null ],
        [ "15.5 Voice Assign", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md120", null ],
        [ "15.6 Timbres y Modos de Programa", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md121", null ]
      ] ],
      [ "16. Frecuencias Exactas del EQ y Escalas del Virtual Patch", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md123", [
        [ "16.1 Frecuencias del Ecualizador", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md124", null ],
        [ "16.2 Escala Máxima del Virtual Patch (Intensidad)", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md125", null ]
      ] ],
      [ "17. Flujo de Señal Completo y Orden de Procesamiento", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md127", null ],
      [ "18. Estructura de Clases Recomendada para JUCE", "md_DOCS_2MS2000__Research__Notes.html#autotoc_md129", null ]
    ] ],
    [ "Korg MS2000 — Especificación de Mensajes SysEx y Offsets Hexadecimales", "md_DOCS_2MS2000__SysEx__Spec.html", [
      [ "1. Estructura de Cabecera SysEx (MS2000 real)", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md132", null ],
      [ "2. Comandos Principales (Sub-status)", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md133", [
        [ "2.1 Empaquetado 7→8 (Korg) y tamaños", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md134", null ]
      ] ],
      [ "3. Mapa del programa real (254 B, <b>unpacked</b>)", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md135", [
        [ "3.1 Bloque de timbre (108 B)", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md136", null ],
        [ "3.2 Cómo se mapea al motor", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md137", null ]
      ] ],
      [ "4. Qué hace ABDMS2000 con esto", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md138", null ],
      [ "5. Bloque nativo del plugin (sistema <span class=\"tt\">native</span>)", "md_DOCS_2MS2000__SysEx__Spec.html#autotoc_md139", null ]
    ] ],
    [ "ReMS2000 — Análisis del Código Lua y Algoritmos MIDI/SysEx", "md_DOCS_2ReMS2000__Lua__Analysis.html", [
      [ "<span class=\"tt\">table_dump</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md142", null ],
      [ "<span class=\"tt\">setGlobalVars</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md144", null ],
      [ "<span class=\"tt\">setGlobalConstants</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md146", null ],
      [ "<span class=\"tt\">startupSequence</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md148", null ],
      [ "<span class=\"tt\">calculateLSBMSB</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md150", null ],
      [ "<span class=\"tt\">midiToProgramData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md152", null ],
      [ "<span class=\"tt\">programToMIDIData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md154", null ],
      [ "<span class=\"tt\">binConversion</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md156", null ],
      [ "<span class=\"tt\">memBlockToTable</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md158", null ],
      [ "<span class=\"tt\">bankIDToName</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md160", null ],
      [ "<span class=\"tt\">packedByteUtils</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md162", null ],
      [ "<span class=\"tt\">normalizeSysExDumpData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md164", null ],
      [ "<span class=\"tt\">getMidiInOut</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md166", null ],
      [ "<span class=\"tt\">bankListPopup</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md168", null ],
      [ "<span class=\"tt\">constructPresetBankMenu</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md170", null ],
      [ "<span class=\"tt\">drawOsc1Waveforms</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md172", null ],
      [ "<span class=\"tt\">setOsc1Waveform</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md174", null ],
      [ "<span class=\"tt\">calculateOscMod</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md176", null ],
      [ "<span class=\"tt\">processOscModData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md178", null ],
      [ "<span class=\"tt\">paintSequencer</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md180", null ],
      [ "<span class=\"tt\">paintSeq2BG</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md182", null ],
      [ "<span class=\"tt\">externalRepaintSequencer</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md184", null ],
      [ "<span class=\"tt\">sequencerMouseDown</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md186", null ],
      [ "<span class=\"tt\">sequencerMouseUp</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md188", null ],
      [ "<span class=\"tt\">sequencerMouseDrag</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md190", null ],
      [ "<span class=\"tt\">selectSequenceByValue</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md192", null ],
      [ "<span class=\"tt\">sequencerMouseDblClick</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md194", null ],
      [ "<span class=\"tt\">paintMidiActivity</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md196", null ],
      [ "<span class=\"tt\">blinkMidiLight</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md198", null ],
      [ "<span class=\"tt\">externalRepaintMidiActivity</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md200", null ],
      [ "<span class=\"tt\">applyVocoderLabels</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md202", null ],
      [ "<span class=\"tt\">paintMIDISettingsButton</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md204", null ],
      [ "<span class=\"tt\">midiSettingsClick</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md206", null ],
      [ "<span class=\"tt\">sendSysExMessage</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md208", null ],
      [ "<span class=\"tt\">inputMIDIParser</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md210", null ],
      [ "<span class=\"tt\">getGlobalMidiChannel</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md212", null ],
      [ "<span class=\"tt\">copySequenceToClipboard</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md214", null ],
      [ "<span class=\"tt\">pasteSequenceFromClipboard</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md216", null ],
      [ "<span class=\"tt\">assertSeqKnobBounds</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md218", null ],
      [ "<span class=\"tt\">getSeqStepMaxVal</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md220", null ],
      [ "<span class=\"tt\">assertSysExFormulas</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md222", null ],
      [ "<span class=\"tt\">assertSeqKnobBoundsByValue</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md224", null ],
      [ "<span class=\"tt\">applyVocoderData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md226", null ],
      [ "<span class=\"tt\">syncVocoderWithBuffer</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md228", null ],
      [ "<span class=\"tt\">setVocoderMode</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md230", null ],
      [ "<span class=\"tt\">initPatchRawData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md232", null ],
      [ "<span class=\"tt\">initPatchData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md234", null ],
      [ "<span class=\"tt\">initPresetBank</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md236", null ],
      [ "<span class=\"tt\">initVocoderBuffer</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md238", null ],
      [ "<span class=\"tt\">resetVocoderControls</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md240", null ],
      [ "<span class=\"tt\">initBufferWithInitPatch</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md242", null ],
      [ "<span class=\"tt\">cycleBanks</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md244", null ],
      [ "<span class=\"tt\">cycleBankValues</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md246", null ],
      [ "<span class=\"tt\">openProgramBankFile</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md248", null ],
      [ "<span class=\"tt\">saveProgramBankFile</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md250", null ],
      [ "<span class=\"tt\">saveProgramToPatchBank</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md252", null ],
      [ "<span class=\"tt\">storeProgramBank</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md254", null ],
      [ "<span class=\"tt\">prepareBulkDump</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md256", null ],
      [ "<span class=\"tt\">getMergedTimbreVocoderData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md258", null ],
      [ "<span class=\"tt\">checkBulkDumpSize</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md260", null ],
      [ "<span class=\"tt\">cutBytesFromDump</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md262", null ],
      [ "<span class=\"tt\">blinkMidiLightTimer</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md264", null ],
      [ "<span class=\"tt\">requestProgramBank</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md266", null ],
      [ "<span class=\"tt\">captureProgramDumpData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md268", null ],
      [ "<span class=\"tt\">captureBulkDumpData</span>", "md_DOCS_2ReMS2000__Lua__Analysis.html#autotoc_md270", null ]
    ] ],
    [ "Espacios de nombres", "namespaces.html", [
      [ "Lista de espacios de nombres", "namespaces.html", "namespaces_dup" ],
      [ "Miembros del espacio de nombres ", "namespacemembers.html", [
        [ "Todos", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Funciones", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "typedefs", "namespacemembers_type.html", null ],
        [ "Enumeraciones", "namespacemembers_enum.html", null ],
        [ "Valores de enumeraciones", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Clases", "annotated.html", [
      [ "Lista de clases", "annotated.html", "annotated_dup" ],
      [ "Índice de clases", "classes.html", null ],
      [ "Jerarquía de clases", "hierarchy.html", "hierarchy" ],
      [ "Miembros de clases", "functions.html", [
        [ "Todos", "functions.html", "functions_dup" ],
        [ "Funciones", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "typedefs", "functions_type.html", null ],
        [ "Enumeraciones", "functions_enum.html", null ],
        [ "Valores de enumeraciones", "functions_eval.html", null ]
      ] ]
    ] ],
    [ "Archivos", "files.html", [
      [ "Lista de archivos", "files.html", "files_dup" ],
      [ "Miembros de los archivos", "globals.html", [
        [ "Todos", "globals.html", null ],
        [ "Funciones", "globals_func.html", null ],
        [ "defines", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"ABDSynthsSysEx_8h.html",
"classABDMS2000_1_1ABDMS2000AudioProcessor.html#a66264fc491a9190c23991f25bd63a426",
"classABDMS2000_1_1BridgeActions.html#a7672d267ab70ad0274fe08be1eb00b79",
"classABDMS2000_1_1HardwareEffects_1_1HardwareDelay.html#a9fabb38c14d0c9f3cd3d3707647e7e5f",
"classABDMS2000_1_1MIDITelemetryManager.html#a2c056fe51909ecfdb77ab7966f6b964a",
"classABDMS2000_1_1MS2KVocoder16Band.html#a4fefdce62709fcc0a99970262dcdfc6b",
"classABDMS2000_1_1NoiseGenerator.html#a3c8a727a4f5cb86e4fc9b37c64407b26",
"classABDMS2000_1_1SynthEngine.html#aefd87fcc901bd333b2eb0180276f65a0",
"classABDMS2000_1_1Tests_1_1RapidSweepTests.html",
"classABDMS2000_1_1VoiceManager.html#a2a77647cc5450eaa85253be818b70a51",
"functions_l.html",
"namespaceABDMS2000.html#a28040a5818d8f8b96a3a4c36b1c4b988",
"namespaceABDMS2000_1_1DSPUtils.html#a811ef7806eb2bb6d1dd6c1625b8f1d18",
"namespaceABDMS2000_1_1ParamIDs.html#a981fa3d3c1aae6d0af6b42fc330ba7cc",
"namespaceABDMS2000_1_1WavetableLoader.html#aa0febd68fc3913738de3da68bb6f17d7",
"structABDMS2000_1_1FirmwareTables_1_1HardwareVoiceOutputFilterCoeffs.html#a9e055becb73975bdba612bee49fa9803",
"structABDMS2000_1_1MS2000HardwareProgram_1_1Field.html#a1d820b29d63cc8002f49025e2ea5909c",
"structABDMS2000_1_1PatchModulationSources.html#a37337034d256f947c477334fded90d7f",
"structABDMS2000_1_1VoiceParameters.html#ac7aaea22cb9f4c6532e348119cb4c170"
];

var SYNCONMSG = 'pulsar para deshabilitar sincronización';
var SYNCOFFMSG = 'pulsar para habilitar sincronización';
var LISTOFALLMEMBERS = 'Lista de todos los miembros';