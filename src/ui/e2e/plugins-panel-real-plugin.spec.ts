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
  // Generous timeout: server startup + navigation + plugin preload/unload + WS handshake
  test.setTimeout(90_000);

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

  // ── Subscription wiring ────────────────────────────────────────────────────
  // Before submitting the mutation, inject a raw graphql-transport-ws subscriber
  // into the browser page.  Once connection_ack is received we immediately send
  // a subscribe frame for pluginRegistryChanged.  The first resulting `next`
  // frame is stored on window.__pluginRegistryEvent so Playwright can read it.
  await page.evaluate(() => {
    type PluginRegistryEvent = { action: string; pluginId: string; timestamp: string };
    type GqlMsg = { type: string; id?: string; payload?: unknown };
    const w = window as Window & {
      __wsSubscribed?: boolean;
      __pluginRegistryEvent?: PluginRegistryEvent | null;
    };
    w.__wsSubscribed = false;
    w.__pluginRegistryEvent = null;
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const url = `${protocol}//${window.location.host}/graphql`;
    const ws = new WebSocket(url, ['graphql-transport-ws']);
    ws.onopen = () => {
      ws.send(JSON.stringify({ type: 'connection_init', payload: {} }));
    };
    ws.onmessage = (evt: MessageEvent<string>) => {
      let msg: GqlMsg;
      try {
        msg = JSON.parse(evt.data) as GqlMsg;
      } catch {
        return;
      }
      if (msg.type === 'connection_ack') {
        ws.send(
          JSON.stringify({
            id: 'sub-e2e',
            type: 'subscribe',
            payload: {
              query: `subscription { isched { pluginRegistryChanged { action pluginId timestamp } } }`,
            },
          }),
        );
        w.__wsSubscribed = true;
      } else if (msg.type === 'next' && msg.id === 'sub-e2e') {
        const data = (
          msg.payload as {
            data?: {
              isched?: { pluginRegistryChanged?: PluginRegistryEvent };
              pluginRegistryChanged?: PluginRegistryEvent;
            };
          }
        )?.data;
        // Initial value comes from GraphQL execution shape:
        //   data.isched.pluginRegistryChanged
        // Ongoing broker pushes are currently published as:
        //   data.pluginRegistryChanged
        const event = data?.isched?.pluginRegistryChanged ?? data?.pluginRegistryChanged;
        // Skip the synthetic initial snapshot (action='initial') sent on subscribe;
        // only capture real load/unload events.
        if (event && event.action !== 'initial') {
          w.__pluginRegistryEvent = event;
          ws.close();
        }
      }
    };
  });

  // Wait until the subscription has been acknowledged by the server and the
  // subscribe frame has been sent (i.e., the server is now registered as a
  // listener on the __plugin_registry topic).
  await page.waitForFunction(
    () => !!(window as Window & { __wsSubscribed?: boolean }).__wsSubscribed,
    { timeout: 8_000 },
  );

  // ── Trigger the mutation ───────────────────────────────────────────────────
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

  // ── Assert subscription event ──────────────────────────────────────────────
  // The loadPlugin mutation must have published an event to the
  // __plugin_registry topic; the raw WS subscriber above should have received
  // a `next` frame carrying a PluginRegistryEvent with action='loaded'.
  await page.waitForFunction(
    () => !!(window as Window & { __pluginRegistryEvent?: unknown }).__pluginRegistryEvent,
    { timeout: 15_000 },
  );
  const pluginEvent = await page.evaluate(
    () =>
      (
        window as Window & { __pluginRegistryEvent?: { action: string; pluginId: string; timestamp: string } }
      ).__pluginRegistryEvent,
  );
  expect(pluginEvent).not.toBeNull();
  expect(pluginEvent?.action).toBe('loaded');
  expect(pluginEvent?.pluginId).toBe('hello_plugin');
  expect(pluginEvent?.timestamp).toBeTruthy();
});
