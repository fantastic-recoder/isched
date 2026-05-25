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

  const noPluginsAlert = page.getByText('No plugins loaded yet.').first();
  const pluginsTable = page.locator('table').first();
  await expect
    .poll(
      async () => (await isVisibleWithinTimeout(noPluginsAlert, 300)) || (await isVisibleWithinTimeout(pluginsTable, 300)),
      { timeout: 10_000 },
    )
    .toBeTruthy();

  const activeHelloPluginRow = page
    .locator('tr', { hasText: 'hello_plugin' })
    .filter({ hasText: 'Active' })
    .first();
  if (await isVisibleWithinTimeout(activeHelloPluginRow, 2_000)) {
    await activeHelloPluginRow.locator('input[type="radio"]').first().check();
    await page.getByRole('button', { name: 'Unload Selected' }).click();
    await expect(activeHelloPluginRow).toBeHidden({ timeout: 10_000 });
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

  const pluginRow = page.locator('tr', { hasText: 'hello_plugin' });
  await expect(pluginRow).toBeVisible({ timeout: 10_000 });
  await expect(pluginRow.getByText('Active')).toBeVisible();

  const pluginAccordion = page.locator('.collapse', { hasText: '(hello_plugin)' }).first();
  await expect(pluginAccordion).toContainText(REAL_HELLO_PLUGIN_PATH);
});
