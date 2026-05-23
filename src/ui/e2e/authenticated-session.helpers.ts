import { expect, type Page } from '@playwright/test';

const APP_ENTRY_PATH = '/isched';

export const ADMIN_EMAIL = 'admin@e2e.test';
export const ADMIN_DISPLAY_NAME = 'E2E Admin';
export const ADMIN_PASSWORD = 'Str0ng!Password2025';
export const BOOTSTRAP_BANNER_TEXT = 'Bootstrap mode active';

export async function bootstrapPlatformAdmin(page: Page): Promise<void> {
  await page.goto(APP_ENTRY_PATH);
  await expect(page).toHaveURL(/\/isched\/bootstrap(?:$|\?)/, { timeout: 10_000 });

  await page.locator('#bs-email').fill(ADMIN_EMAIL);
  await page.locator('#bs-displayName').fill(ADMIN_DISPLAY_NAME);
  await page.locator('#bs-password').fill(ADMIN_PASSWORD);
  await page.locator('#bs-submit').click();

  await page.waitForURL(/\/isched\/(dashboard|login)(?:$|\?)/, { timeout: 15_000 });
  if (page.url().includes('/login')) {
    await logInAsPlatformAdmin(page);
  }
  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 15_000 });
  await expect(page.getByText(BOOTSTRAP_BANNER_TEXT)).toHaveCount(0, { timeout: 10_000 });
}

export async function logInAsPlatformAdmin(page: Page): Promise<void> {
  await page.goto(`${APP_ENTRY_PATH}/login`);
  await page.locator('#email').fill(ADMIN_EMAIL);
  await page.locator('#password').fill(ADMIN_PASSWORD);
  await page.locator('#login-submit').click();
  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 15_000 });
}

export async function ensureAuthenticatedDashboard(page: Page): Promise<void> {
  await page.goto(APP_ENTRY_PATH);
  await page.waitForURL(/\/isched\/(bootstrap|login|dashboard)(?:$|\?)/, { timeout: 15_000 });

  if (page.url().includes('/bootstrap')) {
    await bootstrapPlatformAdmin(page);
    return;
  }

  if (page.url().includes('/login')) {
    await logInAsPlatformAdmin(page);
    return;
  }

  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 10_000 });
}

