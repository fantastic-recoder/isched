import { expect, test, type Page } from '@playwright/test';
import {
  ensureAuthenticatedDashboard,
  logInAsPlatformAdmin,
} from './authenticated-session.helpers';
import { runConcurrentUiBenchmark, type UiBenchmarkOperation } from './performance.helpers';

const RUN_ID = Date.now().toString(36);
const ORGANIZATION_NAME = `Spec 13 Admin Performance ${RUN_ID}`;
const USER_COUNT = 10;

function userFixture(user: number): { email: string; displayName: string } {
  const suffix = String(user + 1).padStart(2, '0');
  return {
    email: `spec13-admin-performance-${RUN_ID}-${suffix}@example.test`,
    displayName: `Spec 13 Admin Performance User ${suffix}`,
  };
}

const ADMIN_PERFORMANCE_MANIFEST: readonly UiBenchmarkOperation[] = Array.from(
  { length: 100 },
  (_, index) => ({
    operationClass:
      index % 4 === 0
        ? 'organization-list'
        : index % 4 === 1
          ? 'organization-user-list'
          : 'user-status-update',
    marker: `admin_benchmark_${String(index).padStart(3, '0')}`,
  }),
);

function graphQLResponse(page: Page, operationName: string) {
  return page.waitForResponse((response) => {
    if (new URL(response.url()).pathname !== '/graphql') return false;
    try {
      const body = response.request().postDataJSON() as { query?: string } | null;
      return body?.query?.includes(operationName) ?? false;
    } catch {
      return false;
    }
  });
}

async function waitForRenderFrame(page: Page): Promise<void> {
  await page.evaluate(() => new Promise<void>((resolve) => requestAnimationFrame(() => resolve())));
}

async function prepareAdminBenchmarkOperation(
  page: Page,
  operation: UiBenchmarkOperation,
  user: number,
  _operationIndex: number,
): Promise<() => Promise<void>> {
  if (operation.operationClass === 'organization-list') {
    return async () => {
      const response = graphQLResponse(page, 'query Organizations');
      await page.getByTestId('shell-nav-organizations').click();
      await response;
      await expect(page.getByRole('heading', { name: 'Organizations' })).toBeVisible();
      await expect(page.locator('#organizations-list')).toContainText(ORGANIZATION_NAME);
      await expect(page.locator('[aria-label="Loading organizations"]')).toHaveCount(0);
      await waitForRenderFrame(page);
    };
  }

  if (operation.operationClass === 'organization-user-list') {
    return async () => {
      const fixture = userFixture(user);
      const response = graphQLResponse(page, 'query Users');
      await page.getByTestId('shell-nav-users').click();
      await response;
      await expect(page.getByTestId('users-page')).toBeVisible();
      await expect(page.locator('#users-list')).toContainText(fixture.email);
      await expect(page.locator('[aria-label="Loading users"]')).toHaveCount(0);
      await page.getByRole('button', { name: `Edit user ${fixture.displayName}` }).click();
      await expect
        .poll(async () => {
          const canDeactivate = await page
            .getByRole('button', { name: 'Deactivate selected user' })
            .isEnabled();
          const canReactivate = await page
            .getByRole('button', { name: 'Reactivate selected user' })
            .isEnabled();
          return canDeactivate || canReactivate;
        })
        .toBe(true);
      await waitForRenderFrame(page);
    };
  }

  return async () => {
    const fixture = userFixture(user);
    const deactivateButton = page.getByRole('button', { name: 'Deactivate selected user' });
    const reactivateButton = page.getByRole('button', { name: 'Reactivate selected user' });
    const deactivating = await deactivateButton.isEnabled();
    const expectedStatus = deactivating ? 'disabled' : 'active';
    const response = graphQLResponse(page, 'mutation UpdateUser');
    await (deactivating ? deactivateButton : reactivateButton).click();
    await response;
    await expect(page.locator('#users-status')).toContainText(
      `${fixture.displayName} is now ${expectedStatus}.`,
    );
    await expect(page.locator('#users-list')).toContainText(deactivating ? 'DISABLED' : 'ACTIVE');
    if (deactivating) {
      await expect(reactivateButton).toBeEnabled();
    } else {
      await expect(deactivateButton).toBeEnabled();
    }
    await waitForRenderFrame(page);
  };
}

test.describe('Admin performance', () => {
  test('meets the independent 10-user, 1,000-operation Admin performance protocol', async ({
    browser,
  }, testInfo) => {
    test.setTimeout(600_000);
    const setupContext = await browser.newContext();
    const users: Array<{ context: Awaited<ReturnType<typeof browser.newContext>>; page: Page }> = [];
    try {
      const setupPage = await setupContext.newPage();
      await ensureAuthenticatedDashboard(setupPage);
      await setupPage.getByTestId('shell-nav-organizations').click();
      await expect(setupPage.getByRole('heading', { name: 'Organizations' })).toBeVisible();
      await setupPage.locator('#org-name-input').fill(ORGANIZATION_NAME);
      await setupPage.locator('#org-create-submit').click();
      await expect(setupPage.locator('#organizations-list')).toContainText(ORGANIZATION_NAME);

      await setupPage.getByTestId('shell-nav-users').click();
      await expect(setupPage.getByTestId('users-page')).toBeVisible();
      await setupPage.locator('#user-org-select').selectOption({ label: ORGANIZATION_NAME });

      for (let user = 0; user < USER_COUNT; user += 1) {
        const fixture = userFixture(user);
        await setupPage.locator('#users-email').fill(fixture.email);
        await setupPage.locator('#users-password').fill('Valid!Password123');
        await setupPage.locator('#users-displayName').fill(fixture.displayName);
        await setupPage.locator('#users-save-submit').click();
        await expect(setupPage.locator('#users-list')).toContainText(fixture.email);
        await setupPage.getByRole('button', { name: 'Clear Form' }).click();
      }

      await setupContext.close();

      for (let user = 0; user < USER_COUNT; user += 1) {
        const fixture = userFixture(user);
        const context = await browser.newContext();
        const page = await context.newPage();
        users.push({ context, page });
        await logInAsPlatformAdmin(page);
        await page.getByTestId('shell-nav-users').click();
        await expect(page.getByTestId('users-page')).toBeVisible();
        await page.locator('#user-org-select').selectOption({ label: ORGANIZATION_NAME });
        await expect(page.locator('#users-list')).toContainText(fixture.email);
        await page.getByRole('button', { name: `Edit user ${fixture.displayName}` }).click();
        await expect(page.getByRole('button', { name: 'Deactivate selected user' })).toBeEnabled();
      }

      const report = await runConcurrentUiBenchmark(
        users.map(({ page }) => page),
        ADMIN_PERFORMANCE_MANIFEST,
        prepareAdminBenchmarkOperation,
        testInfo,
      );
      await testInfo.attach('admin-performance-environment.json', {
        body: JSON.stringify(
          {
            journey: 'Admin',
            fixture: {
              organizationName: ORGANIZATION_NAME,
              userFixtures: Array.from({ length: USER_COUNT }, (_, user) => userFixture(user)),
              dataDirectory: 'isolated Playwright global-setup data directory',
            },
            browser: 'Playwright Chromium',
            manifestClassCounts: Object.fromEntries(
              [...new Set(ADMIN_PERFORMANCE_MANIFEST.map((item) => item.operationClass))].map(
                (operationClass) => [
                  operationClass,
                  ADMIN_PERFORMANCE_MANIFEST.filter((item) => item.operationClass === operationClass)
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
      await setupContext.close();
      await Promise.all(users.map(({ context }) => context.close()));
    }
  });
});
