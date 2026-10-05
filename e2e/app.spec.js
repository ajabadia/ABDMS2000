import { test, expect } from '@playwright/test';

test.describe('ABDMS2000 Visual Regression - Core UI', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForSelector('#app', { state: 'visible', timeout: 15000 });
    await page.waitForTimeout(1500);
  });

  test('Full app - MS2000 theme (default)', async ({ page }) => {
    await expect(page).toHaveScreenshot('app-ms2000-full.png', {
      fullPage: true,
      animations: 'disabled',
    });
  });

  test('Full app - microKORG theme', async ({ page }) => {
    const themeSelect = page.locator('.abd-theme-switcher--select, select[aria-label="Theme"]');
    if (await themeSelect.count() > 0) {
      await themeSelect.selectOption('microkorg');
      await page.waitForTimeout(800);
    }
    await expect(page).toHaveScreenshot('app-microkorg-full.png', {
      fullPage: true,
      animations: 'disabled',
    });
  });

  test('Full app - ADVANCED theme', async ({ page }) => {
    const themeSelect = page.locator('.abd-theme-switcher--select, select[aria-label="Theme"]');
    if (await themeSelect.count() > 0) {
      await themeSelect.selectOption('advanced');
      await page.waitForTimeout(800);
    }
    await expect(page).toHaveScreenshot('app-advanced-full.png', {
      fullPage: true,
      animations: 'disabled',
    });
  });

  test('Dashboard cards visible', async ({ page }) => {
    const cards = page.locator('.dashboard-card, .panel-card');
    await expect(cards).toHaveCount(8);
    if (await cards.count() > 0) {
      await expect(cards.first()).toHaveScreenshot('dashboard-card-osc.png');
    }
  });

  // Los controles COMPARTIDOS que usan la skin ms2000 viven en los cajones
  // laterales (el panel principal monta otra cosa): hay que abrir uno antes de
  // mirar. Y no vale el `if (count() > 0)` de antes: con el selector equivocado
  // el test se quedaba en vacio y pasaba en VERDE sin comprobar nada — que es
  // justo lo que se quiere cazar en un QA de skins.
  test('Knob rendering - MS2000 skin', async ({ page }) => {
    await page.click('#btn-edit-osc1');
    const knob = page.locator('.abd-knob').first();
    await expect(knob).toBeVisible();
    // La skin tiene que estar MONTADA dentro del control, no solo el wrapper.
    await expect(knob.locator('.abd-skin--ms2000')).toHaveCount(1);
    await expect(knob).toHaveScreenshot('knob-ms2000.png', { timeout: 10000 });
  });

  test('Segmented selector - MS2000 skin', async ({ page }) => {
    await page.click('#btn-edit-osc1');
    const segmented = page.locator('.abd-segmented').first();
    await expect(segmented).toBeVisible();
    await expect(segmented).toHaveClass(/abd-segmented--ms2000/);
    await expect(segmented).toHaveScreenshot('segmented-ms2000.png', { timeout: 10000 });
  });

  test('Filmstrip fader - MS2000 skin', async ({ page }) => {
    const fader = page.locator('.abd-slider, .abd-filmstrip-fader').first();
    // ABDMS2000 no monta ningun ABDSlider: el control no existe en la app, asi
    // que se declara SKIP (visible en el informe) en vez de pasar en vacio.
    test.skip((await fader.count()) === 0, 'ABDMS2000 no monta ABDSlider todavia');
    await expect(fader).toHaveScreenshot('filmstrip-fader-ms2000.png', { timeout: 10000 });
  });
});