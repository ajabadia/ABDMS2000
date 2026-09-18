# Casio CZ Series — Fixtures SysEx

## Estructura

```
casio-cz/
├── factory/     # Bancos de fábrica oficiales
├── community/   # Bancos de la comunidad
└── user/        # Bancos de usuarios
```

## Bancos de fábrica generados

| Archivo | Modelo | Bancos | Patches/banco | Tamaño patch |
|---------|--------|--------|---------------|--------------|
| CZ101_Bank_A.syx | CZ-101 | 1 | 16 | 128 bytes |
| CZ1000_Bank_A.syx | CZ-1000 | 1 | 16 | 128 bytes |
| CZ5000_Bank_A.syx | CZ-5000 | 2 | 16 | 128 bytes |
| CZ1_Bank_A.syx .. CZ1_Bank_D.syx | CZ-1 | 4 | 16 | 288 bytes |

## Formato SysEx

- **Fabricante**: 0x44 (Casio)
- **Encoding**: Nibble (cada byte → 2 nibbles: high/low)
- **Checksum**: Suma de todos los nibbles & 0x7F
- **Estructura**: `F0 44 00 00 <modelId> 10 <ch> <nibbles> <checksum> F7`
- **Model IDs**: CZ-101=0x12, CZ-1000=0x13, CZ-5000=0x14, CZ-1=0x15

## Generación

Estos fixtures fueron generados programáticamente usando el `ModelContract` canónico (`Source/Contracts/Models/casio-cz.ts`) para garantizar roundtrip byte-idéntico:

```typescript
parsePatchSysEx(buildPatchSysEx(rawData)) === rawData
```

## Validación

Ejecutar tests de roundtrip:
```bash
pnpm exec vitest run WebUI/tests/unit/casioCzRealFixture.test.js
```

## Licencia

Fixtures generados sintéticamente — sin restricciones de redistribución.
Datos determinísticos basados en `patchDataSize` y addressing canónico del contrato.