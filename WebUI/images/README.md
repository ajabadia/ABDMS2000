# Model Images — Specifications

Directorio de imágenes de sintetizadores para uso en la aplicación.

## Estructura

```
images/models/
├── logos/          ← Logos de fabricantes (SVG)
├── thumbs/         ← Fotos de producto (WebP)
└── README.md       ← Este archivo
```

## Logos de fabricante (`logos/`)

| Archivo | Modelo | Formato | Tamaño recomendado | Uso |
|---|---|---|---|---|
| `casio-logo.svg` | Casio | SVG | 120×40 px (viewBox) | Header de detalle, lista de bancos |
| `roland-logo.svg` | Roland | SVG | 120×40 px (viewBox) | Header de detalle, lista de bancos |
| `korg-logo.svg` | Korg | SVG | 120×40 px (viewBox) | Header de detalle, lista de bancos |
| `behringer-logo.svg` | Behringer | SVG | 120×40 px (viewBox) | Header de detalle, lista de bancos |
| `yamaha-logo.svg` | Yamaha | SVG | 120×40 px (viewBox) | Header de detalle, lista de bancos |

**Requisitos SVG:**
- Formato: SVG 1.1 o superior
- viewBox obligatorio (sin dimensiones fijas en px)
- Sin dependencias externas (fonts, imágenes raster)
- Colores: monocromático o con transparencia
- Preferible: path-based (no text editable)
- Fondo: transparente

## Fotos de producto (`thumbs/`)

**Descargadas (desde Wikimedia Commons):**

| Archivo | Modelo | Formato | Tamaño | Fuente |
|---|---|---|---|---|
| `casio-cz101.jpg` ✅ | Casio CZ-101 | JPG | 33 KB | Wikimedia Commons |
| `roland-juno106.jpg` ✅ | Roland Juno-106 | JPG | 29 KB | Wikimedia Commons |
| `korg-ms2000.jpg` ✅ | Korg MS2000 | JPG | 62 KB | Wikimedia Commons |
| `korg-microkorg.jpg` ✅ | Korg microKORG | JPG | 53 KB | Wikimedia Commons |
| `yamaha-dx7.jpg` ✅ | Yamaha DX7 | JPG | 32 KB | Wikimedia Commons |
| `yamaha-dx7ii.jpg` ✅ | Yamaha DX7II | JPG | 32 KB | Wikimedia Commons |

**Pendientes (sin foto disponible en Wikimedia Commons):**

| Modelo | Notas |
|---|---|
| Casio CZ-1000, CZ-5000, CZ-1 | Sin foto pública disponible |
| Roland Juno-60, Juno-6, HS-60 | Juno-60 disponible, otras pendientes |
| Korg Prophecy | Sin foto pública disponible |
| Behringer DeepMind 12 | Sin foto en Commons, buscar en press kit |
| Behringer Pro-800 | Sin foto en Commons, buscar en press kit |

**Requisitos para fotos nuevas:**
- Formato: JPG o PNG con transparencia
- Resolución: 400×300 px (relación 4:3)
- Máximo: 800×600 px para retina
- Peso máximo: 80 KB por imagen
- Contenido: foto frontal del sintetizador, sin accesorios
- Encuadre: instrumento centrado, visible completo
- Licencia: CC-BY-SA o libre de restricciones

## Cómo añadir un modelo nuevo

1. Colocar el logo SVG en `logos/` con nombre `{fabricante}-logo.svg`
2. Colocar la foto en `thumbs/` con nombre `{modelo-id}.jpg` (o .webp/.png)
3. Actualizar los campos `icon` y `thumbnail` en `Source/Contracts/Models/{modelo}.ts`
4. Ejecutar `npm run generate` para regenerar los contratos Web
5. Las imágenes se servirán automáticamente desde `/images/models/`

**Fuentes recomendadas para fotos:**
- Wikimedia Commons (CC-BY-SA): https://commons.wikimedia.org
- Press kit del fabricante (verificar licencia)
- Fotos propias del hardware

## Acceso desde la app

Las imágenes se sirven estáticas desde la raíz del servidor:

```
/images/models/logos/yamaha-logo.svg
/images/models/thumbs/yamaha-dx7.webp
```

En JavaScript:
```js
const contract = getContract('yamaha-dx7');
const logoUrl = `/images/models/logos/${contract.icon}`;
const thumbUrl = `/images/models/thumbs/${contract.thumbnail}`;
```

## Notas de implementación

- El directorio `vendor/` es el `publicDir` de Vite, por eso las imágenes van aquí
- Si en el futuro se cambia `publicDir`, mover las imágenes a `public/images/models/`
- Los contratos ya definen `icon` y `thumbnail` — solo hay que colocar los archivos
- Las imágenes faltantes se pueden ignorar (la UI muestra placeholder)
