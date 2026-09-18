# Roland Juno Series — Fixtures SysEx

## Estructura

```
roland-juno/
├── factory/     # Bancos de fábrica
├── community/   # Bancos de la comunidad
└── user/        # Bancos de usuarios
```

## Bancos de fábrica generados

| Archivo | Modelo | Bancos | Patches/banco | Tamaño patch |
|---------|--------|--------|---------------|--------------|
| Juno106_Bank_A.syx .. _B.syx | Juno-106 | 2 | 64 | 18 bytes |
| Juno60_Bank_A.syx .. _B.syx | Juno-60 | 2 | 64 | 18 bytes |
| Juno6_Bank_A.syx .. _B.syx | Juno-6 | 2 | 64 | 18 bytes |
| HS60_Bank_A.syx .. _B.syx | HS-60 | 2 | 64 | 18 bytes |

## Formato SysEx

- **Fabricante**: 0x41 (Roland)
- **Checksum**: XOR de todos los bytes (desde device ID hasta último dato), invertido, & 0x7F
- **Estructura**: `F0 41 <ch> <modelId> 12 <bank> <patchNum> <18 datos> <checksum> F7`
- **Model IDs**: Juno-106/HS-60=0x3E, Juno-60=0x3D, Juno-6=0x3C
- **Bancos**: Bank A=0x20, Bank B=0x21

## Generación

Generados con el `ModelContract` canónico (`Source/Contracts/Models/roland-juno.ts`). Roundtrip verificado:

```typescript
parsePatchSysEx(buildPatchSysEx(rawData)) === rawData
```

## Validación

```bash
pnpm exec vitest run WebUI/tests/unit/rolandJunoRealFixture.test.js
```

## Licencia

Fixtures generados sintéticamente — sin restricciones de redistribución.