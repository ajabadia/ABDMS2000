import { defineConfig } from 'vite';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const webUiRoot = path.join(__dirname, 'WebUI');

export default defineConfig({
  root: 'WebUI',
  base: './',
  server: {
    port: 8384,
    strictPort: true,
    fs: {
      allow: ['..', '../..']
    }
  },
  plugins: [
    {
      name: 'ms2000-bankmanager-resolve',
      resolveId(source, importer) {
        if (source === '../../../WebUI/src/app.js' && importer && importer.replace(/\\/g, '/').includes('mountBankManager.js')) {
          const candidate = path.resolve(path.dirname(importer), source);
          if (fs.existsSync(candidate)) return candidate;
          const localFallback = path.join(webUiRoot, 'abdbank', 'src', 'app.js');
          if (fs.existsSync(localFallback)) return localFallback;
        }
        return null;
      }
    },
    {
      name: 'ms2000-shared-cascade',
      transformIndexHtml(html) {
        // Inyecta shared-cascade.css ANTES de themes.css para establecer la
        // cascada de 3 niveles: tokens compartidos (ABDSharedAssets) → overrides
        // del host → customizaciones del MS2000. Vite resuelve el @import de
        // @abdsynths/shared/styles/tokens.css tanto en dev como en vite build.
        if (html.includes('shared-cascade.css')) return html;
        return html.replace(
          '<link rel="stylesheet" href="src/styles/themes.css">',
          '<link rel="stylesheet" href="/src/styles/shared-cascade.css">\n  ' +
          '<link rel="stylesheet" href="src/styles/themes.css">'
        );
      }
    }
  ]
});