# Yamaha DX7 — SysEx Fixtures

Fixtures de prueba para la vertical completa del Yamaha DX7.

## Archivos

| Archivo | Descripción | Tamaño | Tipo |
|---|---|---|---|
| `single-voice.syx` | Una voz VCED (128 bytes de datos) | 137 bytes | Single voice |
| `bulk-32voices.syx` | Banco completo de 32 voces | 4105 bytes | Bulk dump |
| `e-piano-bank.syx` | Banco de 32 voces con nombres de e-piano | 4105 bytes | Bulk dump |
| `multi-voice.syx` | 3 mensajes de voz separados | 411 bytes | Multi-single |

## Formato SysEx DX7

```
Single voice:  F0 43 <ch> 00 09 20 00 [128B VCED] checksum F7
Bulk 32 voice: F0 43 <ch> 00 09 20 01 [32×128B VCED] checksum F7
Dump request:  F0 43 <ch> 00 09 20 00 F7
```

- **Manufacturer ID**: 0x43 (Yamaha)
- **Command**: 0x09 (Bulk)
- **Sub-status**: 0x20 (VCED)
- **Bulk flag**: 0x00=single, 0x01=bulk 32
- **Checksum**: `(128 - sum(payload) % 128) & 0x7F`

## Layout VCED (128 bytes)

```
Bytes 0x00–0x6B: 6 operadores × 18 bytes = 108 bytes
  Cada operador:
    +0..+3:  EG Rate 1–4
    +4..+7:  EG Level 1–4
    +8:      Output Level (0–99)
    +9:      Keyboard Left Scale
    +10:     Keyboard Scale Curve (0–3)
    +11:     Keyboard Rate Scaling (0–3)
    +12:     AM Sensitivity (0–3)
    +13:     Output On/Off (0–1)
    +14:     Freq Mode (0=ratio, 1=fixed)
    +15:     Freq Coarse (0–31)
    +16:     Freq Fine (0–99)
    +17:     Detune (0–14, 7=center)

Bytes 0x6C–0x7F: Parámetros globales (20 bytes)
  +108..+115: Pitch EG Rate 1–4, Level 1–4
  +116: Algorithm (0–31)
  +117: Feedback (0–7)
  +118: Oscillator Sync (0–1)
  +119: LFO Speed (0–99)
  +120: LFO Delay (0–99)
  +121: LFO PM Depth (0–99)
  +122: LFO AM Depth (0–99)
  +123: LFO Waveform (0–4)
  +124: LFO Sync (0–1)
  +125: Pitch Mod Sensitivity (0–7)
  +126: Transpose (0–47)
  +127: Reserved

Patch Name: bytes 0x09–0x12 (10 ASCII chars, null-padded)
```

## Generar fixtures

```bash
node fixtures/sysex/yamaha-dx7/generate-fixtures.mjs
```

## Licencia

Archivos generados programáticamente para testing. Sin restricciones de uso.
