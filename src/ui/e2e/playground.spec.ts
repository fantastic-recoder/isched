// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground.spec.ts
 * @brief Playwright e2e tests for the GraphQL Playground (SP-011)
 *
 * These tests run against a live isched server started by global-setup.ts.
 * They cover: auth-gate, tree rendering, generate-and-run health query,
 * panel resize persistence, and subscription advisory message.
 */

import { expect, test, type Page } from '@playwright/test';
import {
  ensureAuthenticatedDashboard,
  logInAsPlatformAdmin,
} from './authenticated-session.helpers';
import { runConcurrentUiBenchmark, type UiBenchmarkOperation } from './performance.helpers';

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

async function replaceEditorContent(
  page: import('@playwright/test').Page,
  source: string,
): Promise<void> {
  const editor = page.getByTestId('query-editor-host').locator('.cm-content');
  await page.evaluate((replacement) => {
    const content = document.querySelector(
      '[data-testid="query-editor-host"] .cm-content',
    );
    if (!(content instanceof HTMLElement)) {
      throw new Error('The Playground editor is unavailable.');
    }

    content.focus();
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents(content);
    selection?.removeAllRanges();
    selection?.addRange(range);
    if (!document.execCommand('insertText', false, replacement)) {
      throw new Error('The browser could not replace the Playground editor content.');
    }
  }, source);
  await expect(editor).toHaveText(source);
}

const PLAYGROUND_PERFORMANCE_MANIFEST: readonly UiBenchmarkOperation[] = Array.from(
  { length: 100 },
  (_, index) => ({
    operationClass: index % 10 === 0 ? 'mutation-echo' : 'query-system-state',
    marker: `benchmark_${String(index).padStart(3, '0')}`,
  }),
);

async function preparePlaygroundBenchmarkOperation(
  page: Page,
  operation: UiBenchmarkOperation,
  _user: number,
  _operationIndex: number,
): Promise<() => Promise<void>> {
  const document =
    operation.operationClass === 'mutation-echo'
      ? `mutation { ${operation.marker}: isched { echo(message: "${operation.marker}") } }`
      : `query { ${operation.marker}: isched { systemState { seedModeActive } } }`;
  await replaceEditorContent(page, document);

  return async () => {
    const request = page.waitForResponse((response) => {
      if (new URL(response.url()).pathname !== '/graphql') return false;
      try {
        const body = response.request().postDataJSON() as { query?: string } | null;
        return body?.query?.includes(operation.marker) ?? false;
      } catch {
        return false;
      }
    });
    await page.getByTestId('run-query-btn').click();
    await request;
    await expect(page.getByTestId('result-loading')).toHaveCount(0, { timeout: 15_000 });
    await expect(page.getByTestId('result-json')).toContainText(operation.marker, { timeout: 15_000 });
    await expect(page.getByTestId('run-query-btn')).toBeEnabled();
    await page.evaluate(() => new Promise<void>((resolve) => requestAnimationFrame(() => resolve())));
  };
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

  test('provides schema-backed completion, hover, diagnostics, and GraphQL query/mutation execution', async ({ page }) => {
    test.setTimeout(60_000);
    const applicationRequests: string[] = [];
    const applicationWebSockets: string[] = [];
    page.on('request', (request) => {
      if (request.resourceType() === 'xhr' || request.resourceType() === 'fetch') {
        applicationRequests.push(new URL(request.url()).pathname);
      }
    });
    page.on('websocket', (webSocket) => {
      applicationWebSockets.push(new URL(webSocket.url()).pathname);
    });

    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);
    await expect(page.getByTestId('schema-assistance-loading')).toHaveCount(0);
    await expect(page.getByTestId('schema-assistance-unavailable')).toHaveCount(0);

    await replaceEditorContent(page, 'query { isched { org');
    const completionPopup = page.locator('.cm-tooltip-autocomplete');
    await expect(completionPopup).toBeVisible({ timeout: 10_000 });
    await expect(completionPopup).toContainText('organizations');

    const documentedOperation = 'query { isched { systemState { seedModeActive } } }';
    await replaceEditorContent(page, documentedOperation);
    const hoverPoint = await page.evaluate(() => {
      const editorContent = document.querySelector('[data-testid="query-editor-host"] .cm-content');
      if (!editorContent) return null;
      const walker = document.createTreeWalker(editorContent, NodeFilter.SHOW_TEXT);
      let node: Node | null;
      while ((node = walker.nextNode())) {
        const index = node.textContent?.indexOf('seedModeActive') ?? -1;
        if (index >= 0) {
          const range = document.createRange();
          range.setStart(node, index + 3);
          range.setEnd(node, index + 4);
          const bounds = range.getBoundingClientRect();
          return { x: bounds.x + bounds.width / 2, y: bounds.y + bounds.height / 2 };
        }
      }
      return null;
    });
    expect(hoverPoint).not.toBeNull();
    await page.mouse.move(hoverPoint!.x, hoverPoint!.y);
    await expect(page.locator('.cm-graphql-description')).toContainText(
      'True when no active platform administrator exists',
      { timeout: 10_000 },
    );

    await replaceEditorContent(page, 'query { isched { missingField } }');
    await expect(page.locator('.cm-lintRange-error').first()).toBeVisible({ timeout: 10_000 });

    await replaceEditorContent(page, 'query { isched { systemState { seedModeActive } } }');
    const runBtn = page.getByTestId('run-query-btn');
    await runBtn.click();
    await expect(page.getByTestId('result-loading')).toHaveCount(0, { timeout: 15_000 });
    await expect(page.getByTestId('result-json')).toContainText('seedModeActive');

    const organizationName = `Playground E2E ${Date.now()}`;
    await replaceEditorContent(
      page,
      `mutation { isched { createOrganization(input: { name: "${organizationName}" }) { id name status revision } } }`,
    );
    await runBtn.click();
    await expect(page.getByTestId('result-loading')).toHaveCount(0, { timeout: 15_000 });
    await expect(page.getByTestId('result-json')).toContainText(organizationName);

    expect(applicationRequests.length).toBeGreaterThan(1);
    expect(applicationRequests.every((path) => path === '/graphql')).toBe(true);
    expect(applicationWebSockets.every((path) => path === '/graphql')).toBe(true);
  });

  test('shows syntax and schema type diagnostics and retains editor content after request errors', async ({ page }) => {
    await ensureAuthenticatedDashboard(page);
    await openPlaygroundViaShellNav(page);
    await waitForPlaygroundReady(page);

    await replaceEditorContent(page, 'query { isched {');
    await expect(page.locator('.cm-lintRange-error').first()).toBeVisible({ timeout: 10_000 });
    await replaceEditorContent(page, 'query { isched { notARealField } }');
    await expect(page.locator('.cm-lintRange-error').first()).toBeVisible({ timeout: 10_000 });

    await page.route('**/graphql', async (route) => {
      const body = route.request().postDataJSON() as { query?: string } | null;
      if (body?.query?.includes('deliberateFailure')) {
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ errors: [{ message: 'Temporary GraphQL failure' }] }),
        });
        return;
      }
      await route.continue();
    });
    const source = 'query { isched { deliberateFailure } }';
    await replaceEditorContent(page, source);
    await page.getByTestId('run-query-btn').click();
    await expect(page.getByTestId('result-errors')).toContainText('Temporary GraphQL failure');
    await expect(page.getByTestId('query-editor-host')).toContainText('deliberateFailure');
  });

  test('meets the independent 10-user, 1,000-operation Playground performance protocol', async ({
    browser,
  }, testInfo) => {
    test.setTimeout(600_000);
    const setupContext = await browser.newContext();
    const users: Array<{ context: Awaited<ReturnType<typeof browser.newContext>>; page: Page }> = [];
    try {
      const setupPage = await setupContext.newPage();
      await ensureAuthenticatedDashboard(setupPage);
      await setupContext.close();

      for (let user = 0; user < 10; user += 1) {
        const context = await browser.newContext();
        const page = await context.newPage();
        users.push({ context, page });
        await logInAsPlatformAdmin(page);
        await openPlaygroundViaShellNav(page);
        await waitForPlaygroundReady(page);
        await expect(page.getByTestId('schema-assistance-loading')).toHaveCount(0);
        await expect(page.getByTestId('schema-assistance-unavailable')).toHaveCount(0);
      }

      const report = await runConcurrentUiBenchmark(
        users.map(({ page }) => page),
        PLAYGROUND_PERFORMANCE_MANIFEST,
        preparePlaygroundBenchmarkOperation,
        testInfo,
      );
      await testInfo.attach('playground-performance-environment.json', {
        body: JSON.stringify(
          {
            journey: 'Playground',
            fixture: 'isolated data directory with the test-only platform administrator',
            browser: 'Playwright Chromium',
            manifestClassCounts: Object.fromEntries(
              [...new Set(PLAYGROUND_PERFORMANCE_MANIFEST.map((item) => item.operationClass))].map(
                (operationClass) => [
                  operationClass,
                  PLAYGROUND_PERFORMANCE_MANIFEST.filter((item) => item.operationClass === operationClass)
                    .length,
                ],
              ),
            ),
            measuredSampleCount: report.samples.length,
          },
          null,
          2,
        ),
        contentType: 'application/json',
      });

      expect(report.warmupFailures).toEqual([]);
      expect(report.samples).toHaveLength(1_000);
      expect(report.failures).toEqual([]);
      expect(report.aggregateP95Ms).not.toBeNull();
      expect(report.aggregateP95Ms!).toBeLessThan(2_000);
    } finally {
      await Promise.all(users.map(({ context }) => context.close()));
    }
  });
});
