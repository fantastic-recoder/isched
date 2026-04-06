import { expect, test } from '@playwright/test';

// ---------------------------------------------------------------------------
// Credentials — match those used by the bootstrap test which runs first and
// creates the platform admin.  The global-setup starts a fresh server, but
// bootstrap.spec.ts (which always runs in the same suite) creates the admin
// before the RBAC test can log in.
// ---------------------------------------------------------------------------
const ADMIN_EMAIL = 'admin@e2e.test';
const ADMIN_PASSWORD = 'Str0ng!Password2025';

/**
 * Bootstrap the platform (if not yet done) and return a logged-in page.
 * Uses the same GraphQL mutations as the WebUI so the test stays independent.
 */
async function loginAsAdmin(page: import('@playwright/test').Page): Promise<void> {
  await page.goto('/isched');

  // Wait for the Angular router to settle into either the bootstrap or login route.
  await page.waitForURL(/\/isched\/(bootstrap|login)/, { timeout: 15_000 });

  if (page.url().includes('/bootstrap')) {
    // First run — perform bootstrap to create the admin account.
    await page.locator('#bs-email').fill(ADMIN_EMAIL);
    await page.locator('#bs-displayName').fill('E2E Admin');
    await page.locator('#bs-password').fill(ADMIN_PASSWORD);
    await page.locator('#bs-submit').click();
    // After bootstrap the app auto-logs in and redirects to the dashboard.
    await expect(page).toHaveURL(/\/isched\/dashboard/, { timeout: 15_000 });
    return;
  }

  // Already bootstrapped — log in.
  await page.locator('#email').fill(ADMIN_EMAIL);
  await page.locator('#password').fill(ADMIN_PASSWORD);
  await page.locator('#login-submit').click();
  await expect(page).toHaveURL(/\/isched\/dashboard/, { timeout: 15_000 });
}

test.describe('RBAC roles page', () => {
  test('displays built-in platform roles without errors', async ({ page }) => {
    await loginAsAdmin(page);

    // Navigate through the SPA to preserve in-memory auth state.
    await page.locator('[data-testid="quicklink-rbac"]').click();
    await expect(page).toHaveURL(/\/isched\/admin\/rbac/, { timeout: 10_000 });

    // There must be NO GraphQL error alert visible.
    const errorAlert = page.locator('[role="alert"].alert-error');
    await expect(errorAlert).toHaveCount(0);

    // The role list must be present and contain at least the two built-in roles.
    const roleList = page.locator('#rbac-role-list');
    await expect(roleList).toBeVisible({ timeout: 10_000 });

    // Built-in roles seeded at server startup.
    await expect(roleList).toContainText('platform_admin');
    await expect(roleList).toContainText('tenant_admin');
  });

  test('shows role scope badges alongside role names', async ({ page }) => {
    await loginAsAdmin(page);

    await page.locator('[data-testid="quicklink-rbac"]').click();
    await expect(page).toHaveURL(/\/isched\/admin\/rbac/, { timeout: 10_000 });

    const roleList = page.locator('#rbac-role-list');
    await expect(roleList).toBeVisible({ timeout: 10_000 });

    // Each built-in role should have a visible scope badge.
    const scopeBadges = roleList.locator('.badge');
    await expect(scopeBadges).not.toHaveCount(0);
  });
});

