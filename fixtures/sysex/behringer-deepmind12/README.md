# Fixtures — Behringer DeepMind 12

Colección de archivos SysEx reales para validación del parser, importación, exportación y roundtrip del DeepMind 12.

## Estructura

```
fixtures/sysex/behringer-deepmind12/
├── factory/          Bancos de fábrica (v1.0 y v1.1.2)
├── community/        Parches gratuitos de la comunidad
├── user/             Parches de usuarios various
├── commercial/       Parches comerciales (licencia adquirida)
├── unknown/          Bancos de origen desconocido
└── README.md         Este archivo
```

## Formato confirmado

Todos los archivos .syx siguen el framing validado en ABDEep:

```
F0 00 20 32 20 <device> 02 <protocol> <bank> <program> <278 packed bytes> 00 00 F7
```

- Manufacturer ID: `00 20 32`
- Model ID: `0x20`
- Command dump: `0x02`
- Protocol version: `0x07`
- Bank: `0x00–0x07` (A–H)
- Program: `0x00–0x7F` (0–127)
- Message length: 291 bytes
- Packed payload: 278 bytes
- Unpacked patch data: 242 bytes
- Name offset (decoded): 223–238

## Bancos de fábrica

### Factory Banks V1.0

Archivos originales de Behringer para firmware anterior a v1.1.2.

| Archivo | Banco | Patches | Protocolo | SHA-256 |
|---------|-------|---------|-----------|---------|
| Factory Bank A v1.0.syx | A | 128 | 0x07 | `2512dbae...` |
| Factory Bank B v1.0.syx | B | 128 | 0x07 | `590fd88b...` |
| Factory Bank C v1.0.syx | C | 128 | 0x07 | `fe2bb240...` |
| Factory Bank D v1.0.syx | D | 128 | 0x07 | `40446dab...` |

### Factory Banks V1.1.2

Actualización de fábrica compatible con firmware v1.1.2+.

| Archivo | Banco | Patches | Protocolo | SHA-256 |
|---------|-------|---------|-----------|---------|
| Factory Bank A v1.1.2.syx | A | 128 | 0x07 | `21ed77a4...` |
| Factory Bank H v1.1.2.syx | H | 128 | 0x07 | `841099ea...` |

## Comunidad y gratuitos

Parches de distribución libre de productores independientes.

| Archivo | Origen | Patches | SHA-256 |
|---------|--------|---------|---------|
| AE Angelia.syx | Alba Ecstasy | 1 | `cacdf7b8...` |
| AE CinemaDrone.syx | Alba Ecstasy | 1 | `d96b4e52...` |

## Comerciales

Parches con licencia adquirida por el propietario del proyecto.

| Archivo | Origen | Patches | SHA-256 |
|---------|--------|---------|---------|
| 5P_Media_DM12.syx | 5 Pin Media | 128 | `e909d560...` |
| Ambient Mind Vol 1.syx | Alba Ecstasy | 128 | `51208d71...` |

## Usuarios

Parches de usuarios various recopilados para validación.

| Archivo | Origen | Patches | SHA-256 |
|---------|--------|---------|---------|
| 80s.syx | Usuario desconocido | 128 | `cfb101b0...` |
| GROKa.syx | Usuario desconocido | 128 | `05050a14...` |

## Desconocidos

Archivos de origen no determinado.

| Archivo | Mensajes | SHA-256 |
|---------|----------|---------|
| Warmup.syx | 1 | `7056070b...` |
| 80s.syx | 128 | `cfb101b0...` |

## Política de licencia

- Los bancos de fábrica son redistribuibles para testing.
- Los parches comerciales están incluidos bajo licencia adquirida por el propietario del proyecto.
- Los parches de la comunidad y gratuitos se incluyen bajo las licencias originales de distribución.
- Los archivos de usuarios y desconocidos se mantienen para validación técnica.

## Uso en tests

Los fixtures se cargan desde rutas relativas al proyecto:

```javascript
const fixturePath = 'fixtures/sysex/behringer-deepmind12/factory/Factory Bank A v1.0.syx';
```

## Catálogo completo

El catálogo completo de 260 archivos disponibles en `DOCS/dm12-library-catalog.json`.
