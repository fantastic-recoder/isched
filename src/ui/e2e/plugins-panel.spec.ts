import { expect, test } from '@playwright/test';
import { ensureAuthenticatedDashboard } from './authenticated-session.helpers';

type PluginRecord = {
  id: string;
  kind: string;
  name: string;
  version: string;
  description: string;
  libraryPath: string;
  isLoaded: boolean;
  loadedAt: string;
  unloadedAt: string;
};

test('plugins panel supports mocked load + select + unload lifecycle', async ({ page }) => {
  const plugins: PluginRecord[] = [
    {
      id: 'audit-plugin',
      kind: 'REQUEST',
      name: 'Audit Plugin',
      version: '1.2.0',
      description: 'Captures and audits request metadata.',
      libraryPath: '/opt/isched/plugins/libaudit_plugin.so',
      isLoaded: true,
      loadedAt: '2026-05-20T12:00:00Z',
      unloadedAt: '',
    },
  ];

  let loadMutationCount = 0;
  let unloadMutationCount = 0;
  let lastLoadInput: Record<string, unknown> | null = null;
  let lastUnloadId: string | null = null;

  await page.route('**/graphql', async (route) => {
    const payload = route.request().postDataJSON() as { query?: string; variables?: Record<string, unknown> } | null;
    const query = payload?.query ?? '';

    if (query.includes('plugins {')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            isched: {
              plugins,
            },
          },
        }),
      });
      return;
    }

    if (/\bloadPlugin\s*\(/.test(query)) {
      loadMutationCount += 1;
      const input = (payload?.variables?.['input'] as Record<string, unknown> | undefined) ?? {};
      lastLoadInput = input;
      const pluginId = String(input['pluginId'] ?? 'resolver-plugin');
      const kind = String(input['kind'] ?? 'resolver');
      const libraryPath = String(input['libraryPath'] ?? '/opt/isched/plugins/libresolver_plugin.so');

      plugins.push({
        id: pluginId,
        kind,
        name: 'Resolver Plugin',
        version: '0.9.0',
        description: 'Adds resolver extension points.',
        libraryPath,
        isLoaded: true,
        loadedAt: '2026-05-25T10:00:00Z',
        unloadedAt: '',
      });

      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            isched: {
              loadPlugin: {
                success: true,
                pluginId,
                errors: [],
              },
            },
          },
        }),
      });
      return;
    }

    if (/\bunloadPlugin\s*\(/.test(query)) {
      unloadMutationCount += 1;
      const pluginId = String(payload?.variables?.['id'] ?? '');
      lastUnloadId = pluginId;
      const plugin = plugins.find((entry) => entry.id === pluginId);
      if (plugin) {
        plugin.isLoaded = false;
        plugin.unloadedAt = '2026-05-25T10:05:00Z';
      }

      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            isched: {
              unloadPlugin: {
                success: true,
                pluginId,
                errors: [],
              },
            },
          },
        }),
      });
      return;
    }

    await route.continue();
  });

  await ensureAuthenticatedDashboard(page);

  const pluginsToggle = page.getByTestId('dashboard-panel-plugins-toggle');
  await expect(pluginsToggle).toBeVisible();
  if (!(await pluginsToggle.isChecked())) {
    await pluginsToggle.click();
  }

  const unloadSelectedButton = page.getByRole('button', { name: 'Unload Selected' });

  await expect(page.getByRole('button', { name: '+ Load Plugin' })).toBeVisible();
  const auditRow = page.locator('tr', { hasText: 'Audit Plugin' });
  await expect(auditRow).toBeVisible();

  const auditRowRadio = auditRow.locator('input[type="radio"]');
  await expect(auditRowRadio).toBeChecked();
  await expect(unloadSelectedButton).toBeEnabled();

  await page.getByRole('button', { name: '+ Load Plugin' }).click();
  await expect(page.locator('#load_plugin_modal')).toBeVisible();

  await page.locator('#load_plugin_modal input[formControlName="libraryPath"]').fill(
    '/opt/isched/plugins/libresolver_plugin.so',
  );
  await page.locator('#load_plugin_modal select[formControlName="kind"]').selectOption({ label: 'RESOLVER' });
  await page.locator('#load_plugin_modal input[formControlName="pluginId"]').fill('resolver-plugin');
  await page.locator('#load_plugin_modal button[type="submit"]').click();

  await expect.poll(() => loadMutationCount).toBe(1);
  const resolverRow = page.locator('tr', { hasText: 'Resolver Plugin' });
  await expect(resolverRow).toBeVisible();

  await expect(resolverRow.getByText('Active')).toBeVisible();

  const resolverRowRadio = resolverRow.locator('input[type="radio"]');
  await resolverRowRadio.check();
  await expect(resolverRowRadio).toBeChecked();
  await expect(unloadSelectedButton).toBeEnabled();

  await unloadSelectedButton.click();

  const resolverStatusBadge = page.locator('tr', { hasText: 'Resolver Plugin' }).getByText('Inactive');
  await expect(resolverStatusBadge).toBeVisible();

  expect(loadMutationCount).toBe(1);
  expect(unloadMutationCount).toBe(1);
  expect(lastLoadInput).toEqual({
    libraryPath: '/opt/isched/plugins/libresolver_plugin.so',
    kind: 'resolver',
    pluginId: 'resolver-plugin',
  });
  expect(lastUnloadId).toBe('resolver-plugin');
});

test('loadPlugin failure response includes nullable pluginId field', async ({ page }) => {
  let loadFailurePayload: Record<string, unknown> | null = null;

  await page.route('**/graphql', async (route) => {
    const payload = route.request().postDataJSON() as { query?: string } | null;
    const query = payload?.query ?? '';

    if (query.includes('plugins {')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            isched: {
              plugins: [],
            },
          },
        }),
      });
      return;
    }

    if (/\bloadPlugin\s*\(/.test(query)) {
      loadFailurePayload = {
        success: false,
        pluginId: null,
        errors: ["input.kind must be 'request' or 'resolver'"],
      };

      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            isched: {
              loadPlugin: loadFailurePayload,
            },
          },
        }),
      });
      return;
    }

    await route.continue();
  });

  await ensureAuthenticatedDashboard(page);

  const pluginsToggle = page.getByTestId('dashboard-panel-plugins-toggle');
  if (!(await pluginsToggle.isChecked())) {
    await pluginsToggle.click();
  }

  await page.getByRole('button', { name: '+ Load Plugin' }).click();
  await page.locator('#load_plugin_modal input[formControlName="libraryPath"]').fill(
    '/opt/isched/plugins/libbadkind_plugin.so',
  );
  await page.locator('#load_plugin_modal select[formControlName="kind"]').selectOption({ label: 'REQUEST' });
  await page.locator('#load_plugin_modal button[type="submit"]').click();

  await expect(page.locator('#load_plugin_modal').getByText("input.kind must be 'request' or 'resolver'"))
    .toBeVisible();

  expect(loadFailurePayload).not.toBeNull();
  expect(loadFailurePayload).toHaveProperty('pluginId', null);
});

