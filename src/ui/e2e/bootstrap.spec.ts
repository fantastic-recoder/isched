import { expect, test } from '@playwright/test';

test('bootstrap form is visible in seed mode', async ({ page }) => {
  await page.goto('/graphql');

  await expect(page).toHaveURL(/\/graphql\/bootstrap(?:$|\?)/);
  await expect(page.getByRole('heading', { name: 'Initialize Platform' })).toBeVisible();

  await expect(page.getByLabel('Email')).toBeVisible();
  await expect(page.getByLabel('Display Name')).toBeVisible();
  await expect(page.getByLabel('Password')).toBeVisible();

  await expect(page.getByRole('button', { name: 'Complete Bootstrap' })).toBeVisible();
});
