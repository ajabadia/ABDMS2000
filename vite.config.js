import { defineConfig } from 'vite';

export default defineConfig({
  root: 'WebUI',
  base: './',
  server: {
    port: 8384,
    strictPort: true,
    fs: {
      allow: ['..']
    }
  },
  plugins: [
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