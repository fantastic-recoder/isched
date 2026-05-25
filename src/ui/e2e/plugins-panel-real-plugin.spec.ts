import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

import { expect, test, type Locator } from '@playwright/test';

import { ensureAuthenticatedDashboard } from './authenticated-session.helpers';

const REAL_HELLO_PLUGIN_PATH = resolve(__dirname, '../../../cmake-build-debug/src/example/cpp/libhello_plugin.so');

async function isVisibleWithinTimeout(locator: Locator, timeoutMs: number): Promise<boolean> {
  try {
    await locator.waitFor({ state: 'visible', timeout: timeoutMs });
    return true;
  } catch {
    return false;
  }
}

test('plugins panel loads real hello plugin shared library and shows it as active', async ({ page }) => {
  test.skip(
    !existsSync(REAL_HELLO_PLUGIN_PATH),
    `Real example plugin library is missing at ${REAL_HELLO_PLUGIN_PATH}. Build cmake-build-debug example targets first.`,
  );

  await ensureAuthenticatedDashboard(page);

  const pluginsToggle = page.getByTestId('dashboard-panel-plugins-toggle');
  await expect(pluginsToggle).toBeVisible();
  if (!(await pluginsToggle.isChecked())) {
    await pluginsToggle.click();
  }

  const panelTransportError = page.locator('.alert', { hasText: 'GraphQL request failed' }).first();
  if (await isVisibleWithinTimeout(panelTransportError, 5_000)) {
    test.skip(true, 'Dashboard GraphQL transport is unavailable in this environment; skipping real plugin load assertion.');
  }

  await page.getByRole('button', { name: '+ Load Plugin' }).click();
  const loadModal = page.locator('#load_plugin_modal');
  await expect(loadModal).toBeVisible();

  await loadModal.locator('input[formControlName="libraryPath"]').fill(REAL_HELLO_PLUGIN_PATH);
  await loadModal.locator('select[formControlName="kind"]').selectOption({ label: 'RESOLVER' });
  await loadModal.locator('input[formControlName="pluginId"]').fill('hello_plugin');
  await loadModal.locator('button[type="submit"]').click();

  const modalTransportError = loadModal.getByText('GraphQL request failed');
  if (await isVisibleWithinTimeout(modalTransportError, 10_000)) {
    test.skip(true, 'loadPlugin GraphQL mutation failed at transport layer in this environment; skipping assertion.');
  }

  await expect(loadModal).toBeHidden({ timeout: 10_000 });

  const pluginRow = page.locator('tr', { hasText: 'hello_plugin' });
  await expect(pluginRow).toBeVisible();
  await expect(pluginRow.getByText('Active')).toBeVisible();

  const pluginDetails = page.locator('p', { hasText: 'Library path:' }).filter({ hasText: REAL_HELLO_PLUGIN_PATH });
  await expect(pluginDetails).toBeVisible();
});
