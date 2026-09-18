# Korg MS2000 / microKORG / Prophecy — Fixtures SysEx

## Estructura

```
korg-ms2000/
korg-microkorg/
korg-prophecy/
├── factory/     # Bancos de fábrica + singles reales
├── community/
└── user/
```

## Bancos de fábrica (reales — descargados de rhythm-lab.com)

### MS2000 / microKORG (288 bytes raw / patch)

| Modelo | Bancos | Patches/banco | Tamaño raw | Tamaño wire (7→8) |
|--------|--------|---------------|------------|-------------------|
| MS2000 | 8 | 16 | 288 bytes | 336 bytes (336 = 288/7*8) |
| microKORG | 8 | 16 | 288 bytes | 336 bytes |

### Prophecy (535 bytes raw / patch)

| Modelo | Bancos | Patches/banco | Tamaño raw | Tamaño wire (7→8, ctrl al final) |
|--------|--------|---------------|------------|----------------------------------|
| Prophecy | 2 | 64 | 535 bytes | Single: 611 bytes (76×8+3)<br>Bank: 39141 bytes (34240 raw continuo → 39131 wire + F7) |

## Formato SysEx Prophecy (verificado con dumps reales)

- **Fabricante**: 0x42 (Korg)
- **Model ID**: 0x41 (diferente de MS2000/microKORG=0x58)
- **Packing**: 7-to-8 bit con **control byte AL FINAL** de cada grupo (`[7 datos][1 ctrl]`), tail 3 bytes sin ctrl
- **Sin checksum separado** — integridad vía estructura de packing
- **Single dump**: `F0 42 3n 41 40 01 00 <611 packed> F7`
- **Bank dump**: `F0 42 3n 41 4C <10|11> 00 00 00 <stream continuo 64×535> F7`
  - Bank A (addr 0x10): 64 patches (slots 0–63)
  - Bank B (addr 0x11): 64 patches (slots 64–127)
- **CMDs**: 0x40 (Single), 0x4C (Bank), 0x10 (Request single), 0x0E (Request all)

## Contenido factory/

| Archivo | Tipo | Descripción |
|---------|------|-------------|
| `VCS3.SYX` | Single (0x40) | "Very Pink VCS3" |
| `STEELBLL.SYX` | Single (0x40) | "Steel Bell" |
| `70SAW.SYX` | Single (0x40) | "70SAW" |
| `5000HZ.SYX` | Single (0x40) | "5000HZ" |
| `Whiskey.syx` | Single (0x40) | "Whiskey" |
| `A50_Void.syx` | Single 0x4C-variant | Edge case: 621B, 0x4C con addr |
| `Megawave.syx` | Bank 0x4C | Bank A (0x10), 64 patches |
| `Modmodel.syx` | Bank 0x4C | Bank B (0x11), 64 patches |

## Generación / Validación

Los fixtures son **dumps reales** (no sintéticos). El contrato canónico en `Source/Contracts/Models/korg-ms2000.ts` implementa el formato real (packing ctrl-al-final).

```bash
# Tests roundtrip + fixtures reales
pnpm exec vitest run WebUI/tests/unit/korgMs2000RealFixture.test.js
pnpm exec vitest run WebUI/tests/unit/checksumValidation.test.js
```

## Licencia

Dumps obtenidos de http://www.rhythm-lab.com — uso personal/educativo.