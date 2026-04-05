import { expect, test } from '@playwright/test';

// ---------------------------------------------------------------------------
// Credentials used throughout the bootstrap + login flow tests.
// The server is started with a fresh temporary data directory by global-setup,
// so the platform is always in seed / bootstrap mode at the start of a run.
// ---------------------------------------------------------------------------
const ADMIN_EMAIL = 'admin@e2e.test';
const ADMIN_DISPLAY_NAME = 'E2E Admin';
const ADMIN_PASSWORD = 'Str0ng!Password2025'; // ≥ 12 chars, satisfies validator
const BOOTSTRAP_BANNER_TEXT = 'Bootstrap mode active';

// ---------------------------------------------------------------------------
// Test 1 — UI shape check (no state mutation)
// ---------------------------------------------------------------------------
test('bootstrap form is visible in seed mode', async ({ page }) => {
  await page.goto('/isched');

  // The app should redirect to the bootstrap route while seed mode is active.
  await expect(page).toHaveURL(/\/isched\/bootstrap(?:$|\?)/);
  await expect(page.getByRole('heading', { name: 'Initialize Platform' })).toBeVisible();

  await expect(page.locator('#bs-email')).toBeVisible();
  await expect(page.locator('#bs-displayName')).toBeVisible();
  // Use ID to avoid matching the adjacent "Show password" aria-label button.
  await expect(page.locator('#bs-password')).toBeVisible();

  await expect(page.getByRole('button', { name: 'Complete Bootstrap' })).toBeVisible();
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
  await page.goto('/isched');
  await expect(page).toHaveURL(/\/isched\/bootstrap(?:$|\?)/, { timeout: 10_000 });
  await expect(page.getByRole('heading', { name: 'Initialize Platform' })).toBeVisible();

  // ── Step 2: fill in and submit the bootstrap form ──────────────────────
  // Use element IDs to avoid ambiguity with the "Show password" aria-label button.
  await page.locator('#bs-email').fill(ADMIN_EMAIL);
  await page.locator('#bs-displayName').fill(ADMIN_DISPLAY_NAME);
  await page.locator('#bs-password').fill(ADMIN_PASSWORD);

  await page.getByRole('button', { name: 'Complete Bootstrap' }).click();

  // ── Step 3: after successful bootstrap the page auto-logs in and navigates
  //           to the dashboard ─────────────────────────────────────────────
  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 15_000 });
  // The navbar brand is always visible on the dashboard.
  await expect(page.getByText('isched', { exact: false })).toBeVisible();
  // Bootstrap mode should be off after successful completion.
  await expect(page.getByText(BOOTSTRAP_BANNER_TEXT)).toHaveCount(0);

  // ── Step 4: sign out ───────────────────────────────────────────────────
  // The sign-out button triggers a browser confirm() dialog — accept it.
  page.once('dialog', (dialog) => void dialog.accept());
  await page.getByRole('button', { name: 'Sign out' }).click();

  // After sign-out the app should redirect to the login page (seed mode is
  // now inactive, so the bootstrap gate will not apply).
  await expect(page).toHaveURL(/\/isched\/login(?:$|\?)/, { timeout: 10_000 });

  // ── Step 5: log back in with the credentials created during bootstrap ──
  await page.locator('#email').fill(ADMIN_EMAIL);
  await page.locator('#password').fill(ADMIN_PASSWORD);
  await page.getByRole('button', { name: 'Sign In' }).click();

  // Should arrive at the dashboard again.
  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 15_000 });
  await expect(page.getByText('isched', { exact: false })).toBeVisible();
  await expect(page.getByText(BOOTSTRAP_BANNER_TEXT)).toHaveCount(0);
});
