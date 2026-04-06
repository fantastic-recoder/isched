import { expect, test } from '@playwright/test';
import { ADMIN_DISPLAY_NAME, ensureAuthenticatedDashboard } from './authenticated-session.helpers';

test('shared authenticated shell shows nav, active route state, current user, and latest user-load digest', async ({
  page,
}) => {
  await page.route('**/graphql', async (route) => {
    const payload = route.request().postDataJSON() as { query?: string } | null;
    const query = payload?.query ?? '';

    if (query.includes('query Organizations(')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            organizations: {
              nodes: [
                {
                  id: 'org-shell',
                  name: 'Shell Org',
                  status: 'ACTIVE',
                  revision: 3,
                  updatedAt: '2026-04-06T10:00:00Z',
                },
              ],
              pageInfo: { number: 1, size: 25, totalElements: 1, totalPages: 1 },
            },
          },
        }),
      });
      return;
    }

    if (query.includes('query Users(')) {
      await page.waitForTimeout(200);
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          data: {
            users: {
              nodes: [
                {
                  id: 'user-shell-1',
                  organizationId: 'org-shell',
                  loginId: 'shell.user',
                  displayName: 'Shell User',
                  status: 'ACTIVE',
                  revision: 1,
                  updatedAt: '2026-04-06T10:02:00Z',
                  roleAssignments: [],
                },
              ],
              pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
            },
          },
        }),
      });
      return;
    }

    await route.continue();
  });

  await ensureAuthenticatedDashboard(page);

  await expect(page.getByTestId('authenticated-shell')).toBeVisible();
  await expect(page.getByTestId('shell-logo')).toBeVisible();
  await expect(page.getByTestId('shell-status-bar')).toBeVisible();
  await expect(page.getByTestId('shell-current-user')).toContainText(ADMIN_DISPLAY_NAME);
  await expect(page.getByRole('button', { name: 'Sign out' })).toHaveCount(1);

  await page.getByTestId('shell-nav-users').click();
  await expect(page).toHaveURL(/\/isched\/admin\/users(?:$|\?)/, { timeout: 10_000 });
  await expect(page.getByTestId('shell-nav-users')).toHaveAttribute('aria-current', 'page');
  await expect(page.getByTestId('users-page')).toBeVisible();
  await expect(page.getByTestId('users-org-select')).toBeVisible();
  await expect(page.getByTestId('shell-status-digest')).toContainText('Loading organization users');
  await expect(page.getByTestId('shell-status-digest')).toContainText('Organization users loaded');

  await page.getByTestId('shell-nav-organizations').click();
  await expect(page).toHaveURL(/\/isched\/admin\/organizations(?:$|\?)/, { timeout: 10_000 });
  await expect(page.getByTestId('shell-nav-organizations')).toHaveAttribute('aria-current', 'page');

  await page.getByTestId('shell-nav-rbac').click();
  await expect(page).toHaveURL(/\/isched\/admin\/rbac(?:$|\?)/, { timeout: 10_000 });
  await expect(page.getByTestId('shell-nav-rbac')).toHaveAttribute('aria-current', 'page');

  await page.getByTestId('shell-nav-dashboard').click();
  await expect(page).toHaveURL(/\/isched\/dashboard(?:$|\?)/, { timeout: 10_000 });
  await expect(page.getByTestId('shell-nav-dashboard')).toHaveAttribute('aria-current', 'page');
});

