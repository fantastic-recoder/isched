import { expect, test } from '@playwright/test';
import { ensureAuthenticatedDashboard } from './authenticated-session.helpers';

test.describe('Organization and user administration', () => {
  test('creates, updates, and changes organization and user status through GraphQL', async ({ page }) => {
    test.setTimeout(120_000);
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
    const organizationName = `Admin E2E ${Date.now()}`;
    await page.getByTestId('shell-nav-organizations').click();
    await expect(page.getByRole('heading', { name: 'Organizations' })).toBeVisible();
    await page.locator('#org-name-input').fill(organizationName);
    await page.locator('#org-create-submit').click();
    await expect(page.locator('#organizations-list')).toContainText(organizationName);
    await expect(page.locator('#organizations-status')).toContainText(
      `Created organization ${organizationName}.`,
    );

    await page.getByRole('button', { name: `Edit organization ${organizationName}` }).click();
    await page.locator('#org-name-input').fill(`${organizationName} Updated`);
    await page.locator('#org-create-submit').click();
    await expect(page.locator('#organizations-list')).toContainText(`${organizationName} Updated`);

    await page.getByRole('button', { name: `Deactivate organization ${organizationName} Updated` }).click();
    await expect(page.locator('#organizations-list')).toContainText('SUSPENDED');
    await page.getByRole('button', { name: `Reactivate organization ${organizationName} Updated` }).click();
    await expect(page.locator('#organizations-list')).toContainText('ACTIVE');

    await page.getByTestId('shell-nav-users').click();
    await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible();
    await page.locator('#user-org-select').selectOption({ label: `${organizationName} Updated` });
    await expect(page.locator('#users-list')).toContainText(
      'No users match the current server-side query.',
    );
    const email = `admin-e2e-${Date.now()}@example.test`;
    await page.locator('#users-email').fill(email);
    await page.locator('#users-password').fill('Valid!Password123');
    await page.locator('#users-displayName').fill('Admin E2E User');
    await page.locator('#users-save-submit').click();
    await expect(page.locator('#users-list')).toContainText(email);
    await expect(page.locator('#users-status')).toContainText('User changes saved.');

    await page.locator('#users-displayName').fill('Admin E2E User Updated');
    await page.locator('#users-save-submit').click();
    await expect(page.locator('#users-list')).toContainText('Admin E2E User Updated');
    await page.getByRole('button', { name: 'Deactivate selected user' }).click();
    await expect(page.locator('#users-list')).toContainText('DISABLED');
    await page.getByRole('button', { name: 'Reactivate selected user' }).click();
    await expect(page.locator('#users-list')).toContainText('ACTIVE');

    expect(applicationRequests.length).toBeGreaterThan(0);
    expect(applicationRequests.every((path) => path === '/graphql')).toBe(true);
    expect(applicationWebSockets.every((path) => path === '/graphql')).toBe(true);
  });
});
