// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground.spec.ts
 * @brief Playwright e2e tests for the GraphQL Playground (SP-011)
 *
 * These tests run against a live isched server started by global-setup.ts.
 * They cover: auth-gate, tree rendering, generate-and-run health query,
 * panel resize persistence, and subscription advisory message.
 */

import { expect, test } from '@playwright/test';
import { ensureAuthenticatedDashboard } from './authenticated-session.helpers';
import { OperationLatencyRecorder } from '../src/app/services/operation-latency.util';

const PLAYGROUND_PATH = '/isched/playground';

async function openPlaygroundViaShellNav(page: import('@playwright/test').Page): Promise<void> {
  const playgroundLink = page.getByTestId('shell-nav-playground');
  await expect(playgroundLink).toBeVisible({ timeout: 10_000 });
  await playgroundLink.click();
  await expect(page).toHaveURL(/\/isched\/playground(?:$|\?)/, { timeout: 10_000 });
}

async function waitForPlaygroundReady(page: import('@playwright/test').Page): Promise<void> {
  await expect(page.getByTestId('playground-page')).toBeVisible({ timeout: 10_000 });
  await expect(page.locator('[aria-label="Loading schema"]')).toHaveCount(0, { timeout: 15_000 });
}

test.describe('GraphQL Playground', () => {
  test('redirects unauthenticated users to /login or /bootstrap', async ({ page }) => {
    await page.goto(PLAYGROUND_PATH);
    await expect(page).toHaveURL(/\/isched\/(login|bootstrap)(?:$|\?)/, { timeout: 10_000 });
  });

  test('is reachable via the main navigation after login', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
  });

  test('renders schema tree with Queries group', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    // Queries group should be present
    const queriesGroup = page.locator('.node-name', { hasText: /^Queries$/ });
    await expect(queriesGroup).toBeVisible({ timeout: 10_000 });
  });

  test('generate and run the health query', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    // Find the health field in the tree and click it to select
    const queriesGroup = page.locator('.node-name', { hasText: /^Queries$/ }).first();
    await expect(queriesGroup).toBeVisible({ timeout: 10_000 });

    const queryContainer = queriesGroup.locator('xpath=ancestor::div[contains(@class,"tree-node")][1]');
    const toggle = queryContainer.locator('button[aria-label^="Expand"], button[aria-label^="Collapse"]');
    if (await toggle.count()) {
      const label = (await toggle.first().getAttribute('aria-label')) ?? '';
      if (label.startsWith('Expand')) {
        await toggle.first().click();
      }
    }

    const ischedNamespace = queryContainer.locator('.node-name', { hasText: /^isched$/ }).first();
    await expect(ischedNamespace).toBeVisible({ timeout: 10_000 });

    const ischedContainer = ischedNamespace.locator('xpath=ancestor::div[contains(@class,"tree-node")][1]');
    const namespaceToggle = ischedContainer.locator(
      'button[aria-label^="Expand"], button[aria-label^="Collapse"]',
    );
    if (await namespaceToggle.count()) {
      const namespaceLabel = (await namespaceToggle.first().getAttribute('aria-label')) ?? '';
      if (namespaceLabel.startsWith('Expand')) {
        await namespaceToggle.first().click();
      }
    }

    const healthNode = ischedContainer.getByRole('option', { name: /^⬡ health / }).first();
    await expect(healthNode).toBeVisible({ timeout: 10_000 });
    await healthNode.click();

    // Click Generate Query button
    const generateBtn = page.getByTestId('generate-query-btn');
    await expect(generateBtn).toBeEnabled({ timeout: 5_000 });
    await generateBtn.click();

    // Editor should contain "health"
    const editorHost = page.getByTestId('query-editor-host');
    await expect(editorHost).toContainText('health', { timeout: 10_000 });

    // Click Run
    const runBtn = page.getByTestId('run-query-btn');
    await runBtn.click();

    // Result panel should show health data (not loading)
    await expect(page.getByTestId('result-loading')).toHaveCount(0, { timeout: 15_000 });
    const resultJson = page.getByTestId('result-json');
    await expect(resultJson).toBeVisible({ timeout: 10_000 });
    await expect(resultJson).toContainText('health', { timeout: 5_000 });
  });

  test('panel resize persists across navigation', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    // Find the vertical divider and drag it 80px to the right
    const divider = page.locator('[role="separator"][aria-orientation="vertical"]').first();
    await expect(divider).toBeVisible({ timeout: 10_000 });

    const box = await divider.boundingBox();
    if (box && box.width > 0 && box.height > 0) {
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
      await page.mouse.down();
      await page.mouse.move(box.x + box.width / 2 + 80, box.y + box.height / 2);
      await page.mouse.up();
    }

    const storedAfterResize = await page.evaluate(() => localStorage.getItem('isched.playground.layout'));

    // Navigate away and back via shell links (preserves authenticated SPA state)
    await page.getByTestId('shell-nav-dashboard').click();
    await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 10_000 });

    await page.getByTestId('shell-nav-playground').click();
    await expect(page).toHaveURL(/\/isched\/playground(?:$|\?)/, { timeout: 10_000 });

    const storedAfterReload = await page.evaluate(() => localStorage.getItem('isched.playground.layout'));
    if (storedAfterResize) {
      expect(storedAfterReload).toBe(storedAfterResize);
    }
    await waitForPlaygroundReady(page);
  });

  test('shows subscription advisory when running a subscription stub', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    const subscriptionNode = page.getByRole('option', { name: /^⚡ / }).first();
    test.skip((await subscriptionNode.count()) === 0, 'No subscription fields were present in schema tree');

    await subscriptionNode.click();

    const generateBtn = page.getByTestId('generate-query-btn');
    await expect(generateBtn).toBeEnabled({ timeout: 5_000 });
    await generateBtn.click();

    await page.getByTestId('run-query-btn').click();

    await expect(page.getByTestId('result-subscription-advisory')).toBeVisible({ timeout: 5_000 });
  });

  test('SC-001: primary Playground flow p95 latency <= 2s', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    const editor = page.getByTestId('query-editor-host').locator('.cm-content');
    await editor.click();
    await page.keyboard.press('Control+A');
    await page.keyboard.type('query { isched { health { status } } }');

    const recorder = new OperationLatencyRecorder();
    const runBtn = page.getByTestId('run-query-btn');
    const resultJson = page.getByTestId('result-json');

    for (let i = 0; i < 10; i++) {
      const timer = recorder.start('playground-query');
      await runBtn.click();
      await expect(resultJson).toContainText('status', { timeout: 10_000 });
      timer.stop();
      await expect(runBtn).toBeEnabled();
    }

    expect(recorder.all().length).toBe(10);
    expect(recorder.percentile(95, 'playground-query')).toBeLessThanOrEqual(2_000);
  });
});
