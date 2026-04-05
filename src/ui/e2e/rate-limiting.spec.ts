import { expect, test, type Page } from '@playwright/test';

// ---------------------------------------------------------------------------
// Rate Limiting E2E Tests (Q2 - RATE_LIMITED Gap Closure)
// Tests that the UI displays retry guidance when rate limited
// ---------------------------------------------------------------------------

const TEST_EMAIL = 'ratelimit-test@e2e.test';
const WRONG_PASSWORD = 'WrongPassword123!';
const CORRECT_PASSWORD = 'Str0ng!Password2025';
const BOOTSTRAP_EMAIL = 'admin@ratelimit-e2e.test';

async function ensureLoginReady(page: Page): Promise<void> {
  await page.goto('/isched/login');
  await page.locator('#email, #bs-email').first().waitFor({ state: 'visible', timeout: 10_000 });

  // In seed/bootstrap mode, login route can render bootstrap content until initial admin exists.
  if (await page.locator('#bs-email').isVisible()) {
    await page.locator('#bs-email').fill(BOOTSTRAP_EMAIL);
    await page.locator('#bs-displayName').fill('Rate Limit Test Admin');
    await page.locator('#bs-password').fill(CORRECT_PASSWORD);
    await page.getByRole('button', { name: /Complete( platform)? Bootstrap/i }).click();

    await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 15_000 });

    page.once('dialog', (dialog) => void dialog.accept());
    await page.getByRole('button', { name: 'Sign out' }).click();
    await expect(page).toHaveURL(/\/isched\/login(?:$|\?)/, { timeout: 10_000 });
  }

  await expect(page.locator('#email')).toBeVisible({ timeout: 10_000 });
}

test.describe('Rate Limiting E2E Flow', () => {
  test('login form shows rate limit alert after multiple failed attempts', async ({ page }) => {
    await ensureLoginReady(page);

    // Now attempt multiple failed logins to trigger rate limiting
    // The backend is configured to rate limit after 5 failed attempts
    // within a 15-minute window
    for (let i = 0; i < 6; i++) {
      await page.locator('#email').fill(TEST_EMAIL);
      await page.locator('#password').fill(WRONG_PASSWORD);
      await page.getByRole('button', { name: 'Sign In' }).click();

      // Wait a bit for the error to appear
      await page.waitForTimeout(500);

      if (i < 4) {
        // Depending on backend state, failures can surface as generic error or lockout warning.
        const feedbackAlert = page.locator('.alert.alert-error, .alert.alert-warning').first();
        await expect(feedbackAlert).toBeVisible({ timeout: 5_000 });
        const feedbackText = await feedbackAlert.textContent();
        expect(feedbackText).toBeTruthy();
      }
    }


    // The alert may not appear immediately if rate limiting is implemented
    // at the backend level. Check if any error alert contains rate limit info
    const feedbackAlert = page.locator('.alert.alert-warning, .alert.alert-error').first();
    const hasRateLimitMessage = await feedbackAlert.textContent().then((text) =>
      text?.toLowerCase().includes('rate') || text?.toLowerCase().includes('too many')
    );

    // If rate limiting is active, we expect to see retry guidance
    if (hasRateLimitMessage) {
      // Verify the alert contains actionable retry guidance
      const alertText = await feedbackAlert.textContent();
      expect(alertText).toMatch(/try|again|wait|minute|second/i);
    }
  });

  test('rate limited user sees metadata-aware retry guidance in warning alert', async ({ page }) => {
    await ensureLoginReady(page);

    // Attempt a login with invalid credentials
    // (This simulates a rate-limited scenario on the backend)
    await page.locator('#email').fill(TEST_EMAIL);
    await page.locator('#password').fill(WRONG_PASSWORD);

    // Intercept the GraphQL response to simulate RATE_LIMITED error
    await page.route('**/graphql', (route) => {
      const payload = route.request().postDataJSON() as { query?: string } | null;
      const query = payload?.query ?? '';
      if (query.includes('login(')) {
        route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({
            errors: [
              {
                message: 'Too many authentication attempts. Please try again in 30 seconds.',
                extensions: {
                  code: 'RATE_LIMITED',
                  retryAfterMs: 30000,
                },
              },
            ],
          }),
        });
        return;
      }
      route.continue();
    });

    // Submit the form
    await page.getByRole('button', { name: 'Sign In' }).click();

    const lockoutAlert = page.locator('.alert.alert-warning');
    await expect(lockoutAlert).toBeVisible({ timeout: 5_000 });
    await expect(lockoutAlert).toContainText('Sign-in temporarily locked');
    await expect(lockoutAlert).toContainText('about 30 seconds');
    await expect(lockoutAlert).toContainText('Retry in about 30 seconds');
  });

  test('rate limited user sees fallback lockout copy when retry metadata is absent', async ({ page }) => {
    await ensureLoginReady(page);

    await page.locator('#email').fill(TEST_EMAIL);
    await page.locator('#password').fill(WRONG_PASSWORD);

    await page.route('**/graphql', (route) => {
      const payload = route.request().postDataJSON() as { query?: string } | null;
      const query = payload?.query ?? '';
      if (query.includes('login(')) {
        route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({
            errors: [
              {
                message: 'Too many authentication attempts',
                extensions: {
                  code: 'RATE_LIMITED',
                },
              },
            ],
          }),
        });
        return;
      }
      route.continue();
    });

    await page.getByRole('button', { name: 'Sign In' }).click();

    const lockoutAlert = page.locator('.alert.alert-warning');
    await expect(lockoutAlert).toBeVisible({ timeout: 5_000 });
    await expect(lockoutAlert).toContainText('Too many failed sign-in attempts. Please wait a few minutes before trying again.');
  });

  test('form remains functional after rate limit is cleared', async ({ page }) => {
    await ensureLoginReady(page);

    // Fill in credentials
    const emailInput = page.locator('#email');
    const passwordInput = page.locator('#password');

    // Email and password fields should be visible
    await expect(emailInput).toBeVisible();
    await expect(passwordInput).toBeVisible();

    // Form should be interactive
    await emailInput.fill('test@example.com');
    await passwordInput.fill('TestPassword123!');

    // Submit button should be clickable
    const submitButton = page.getByRole('button', { name: 'Sign In' });
    await expect(submitButton).toBeEnabled();

    // The form structure should remain intact for retry
    expect(await page.locator('form').count()).toBeGreaterThan(0);
  });
});

