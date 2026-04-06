import { expect, test } from '@playwright/test';
import {
  ADMIN_DISPLAY_NAME,
  ADMIN_EMAIL,
  ADMIN_PASSWORD,
  BOOTSTRAP_BANNER_TEXT,
  bootstrapPlatformAdmin,
  logInAsPlatformAdmin,
} from './authenticated-session.helpers';
import { APP_ENTRY_PATH } from './global-setup';

// ---------------------------------------------------------------------------
// Shared bootstrap/login credentials and flows are centralized in
// authenticated-session.helpers.ts so other smoke tests can reuse them.
// ---------------------------------------------------------------------------

const assertDashboardMinimumContent = async (page: import('@playwright/test').Page) => {
  await expect(page.locator('[data-testid="dashboard-health-badge"]')).toContainText('Healthy');

  const organizationsLink = page.locator('[data-testid="quicklink-organizations"]');
  const usersLink = page.locator('[data-testid="quicklink-users"]');
  const rbacLink = page.locator('[data-testid="quicklink-rbac"]');

  await expect(organizationsLink).toBeVisible();
  await expect(usersLink).toBeVisible();
  await expect(rbacLink).toBeVisible();

  await expect(organizationsLink).toHaveAttribute('href', /\/admin\/organizations$/);
  await expect(usersLink).toHaveAttribute('href', /\/admin\/users$/);
  await expect(rbacLink).toHaveAttribute('href', /\/admin\/rbac$/);
};

// ---------------------------------------------------------------------------
// Test 1 — UI shape check (no state mutation)
// ---------------------------------------------------------------------------
test('bootstrap form is visible in seed mode', async ({ page }) => {
  await page.goto(APP_ENTRY_PATH);

  // The app should redirect to the bootstrap route while seed mode is active.
  await expect(page).toHaveURL(/\/isched\/bootstrap(?:$|\?)/);
  await expect(page.getByRole('heading', { name: 'Initialize Platform' })).toBeVisible();

  await expect(page.locator('#bs-email')).toBeVisible();
  await expect(page.locator('#bs-displayName')).toBeVisible();
  // Use ID to avoid matching the adjacent "Show password" aria-label button.
  await expect(page.locator('#bs-password')).toBeVisible();

  await expect(page.locator('#bs-submit')).toBeVisible();
});

test('redirects to sign-in with a bootstrap-unavailable notice when bootstrap completion becomes unavailable', async ({ page }) => {
  await page.route('**/graphql', (route) => {
    const payload = route.request().postDataJSON() as { query?: string } | null;
    const query = payload?.query ?? '';

    if (query.includes('bootstrapPlatformAdmin(')) {
      route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          errors: [
            {
              message: 'Bootstrap is no longer available',
              extensions: {
                code: 'CONFLICT',
              },
            },
          ],
        }),
      });
      return;
    }

    route.continue();
  });

  await page.goto(APP_ENTRY_PATH);
  await expect(page).toHaveURL(/\/isched\/bootstrap(?:$|\?)/, { timeout: 10_000 });

  await page.locator('#bs-email').fill(ADMIN_EMAIL);
  await page.locator('#bs-displayName').fill(ADMIN_DISPLAY_NAME);
  await page.locator('#bs-password').fill(ADMIN_PASSWORD);
  await page.locator('#bs-submit').click();

  await expect(page).toHaveURL(/\/isched\/login(?:$|\?)/, { timeout: 10_000 });
  const loginAlert = page.locator('[data-testid="login-alert"]');
  await expect(loginAlert).toBeVisible();
  await expect(loginAlert).toContainText('Bootstrap already completed');
  await expect(loginAlert).toContainText('existing platform administrator account');
});

// ---------------------------------------------------------------------------
// Test 2 — Complete bootstrap, verify auto-login, sign out, log back in
//
// This test intentionally mutates server state (creates the first admin user).
// It MUST run after test 1, which only reads state.  Playwright respects the
// declaration order within a file when workers = 1 and fullyParallel = false.
// ---------------------------------------------------------------------------
test('complete bootstrap, auto-login, sign out, and log back in', async ({ page }) => {
  // ── Step 1: navigate to app → should still be in bootstrap / seed mode ──
  await bootstrapPlatformAdmin(page);

  // The navbar brand is always visible on the dashboard.
  await expect(page.getByText('isched', { exact: false })).toBeVisible();
  await assertDashboardMinimumContent(page);

  // ── Step 4: sign out ───────────────────────────────────────────────────
  // The sign-out button triggers a browser confirm() dialog — accept it.
  page.once('dialog', (dialog) => void dialog.accept());
  await page.getByRole('button', { name: 'Sign out' }).click();

  // After sign-out the app should redirect to the login page (seed mode is
  // now inactive, so the bootstrap gate will not apply).
  await expect(page).toHaveURL(/\/isched\/login(?:$|\?)/, { timeout: 10_000 });

  // ── Step 5: log back in with the credentials created during bootstrap ──
  await logInAsPlatformAdmin(page);

  // Should arrive at the dashboard again.
  await expect(page.getByText('isched', { exact: false })).toBeVisible();
  await expect(page.getByText(BOOTSTRAP_BANNER_TEXT)).toHaveCount(0);
  await assertDashboardMinimumContent(page);
});
