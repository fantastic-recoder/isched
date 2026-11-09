import { expect, test, type Page } from '@playwright/test';
import { ensureAuthenticatedDashboard } from './authenticated-session.helpers';

type GraphQLResponse = { data?: unknown; errors?: Array<{ message?: string }> };

async function submitGraphQLWithoutCsrf(page: Page, name: string): Promise<GraphQLResponse> {
  return page.evaluate(async (organizationName) => {
    const response = await fetch('/graphql', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        query: `mutation { isched { createOrganization(input: { name: "${organizationName}" }) { id } } }`,
      }),
    });
    return (await response.json()) as GraphQLResponse;
  }, name);
}

test.describe('Browser cookie, Origin, and CSRF security', () => {
  test('uses the strict HttpOnly cookie, rejects invalid Origins, and blocks mutation side effects without CSRF', async ({
    page,
    request,
  }) => {
    test.setTimeout(60_000);
    const graphqlResponses: Array<{ requestText: string; body: GraphQLResponse }> = [];
    const graphqlRequestHeaders: Array<Record<string, string>> = [];
    const graphqlRequestUrls: string[] = [];
    const responseTasks: Promise<void>[] = [];
    const requestHeaderTasks: Promise<void>[] = [];
    page.on('response', async (response) => {
      if (new URL(response.url()).pathname !== '/graphql') return;
      const requestText = response.request().postData() ?? '';
      responseTasks.push((async () => {
        try {
          graphqlResponses.push({ requestText, body: (await response.json()) as GraphQLResponse });
        } catch {
          // A non-JSON network failure is asserted by its UI path, not here.
        }
      })());
    });
    page.on('request', (browserRequest) => {
      if (new URL(browserRequest.url()).pathname === '/graphql') {
        graphqlRequestUrls.push(browserRequest.url());
        requestHeaderTasks.push(
          browserRequest.allHeaders().then((headers) => graphqlRequestHeaders.push(headers)).then(() => {}),
        );
      }
    });

    await ensureAuthenticatedDashboard(page);
    await Promise.all(responseTasks);
    const origin = new URL(page.url()).origin;
    const cookies = await page.context().cookies(origin);
    const sessionCookie = cookies.find((cookie) => cookie.name === 'isched_session');
    expect(sessionCookie).toBeDefined();
    expect(sessionCookie).toMatchObject({
      secure: true,
      httpOnly: true,
      sameSite: 'Strict',
    });
    expect(await page.evaluate(() => document.cookie)).not.toContain('isched_session');

    const authResponses = graphqlResponses.filter(({ requestText }) =>
      /login\(|bootstrapPlatformAdmin\(/.test(requestText),
    );
    expect(authResponses.length).toBeGreaterThan(0);
    expect(JSON.stringify(authResponses)).not.toMatch(/eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+/);
    expect(JSON.stringify(authResponses)).not.toMatch(/"accessToken"\s*:/);

    const postBody = { query: 'query OriginCheck { __typename }' };
    const validOriginResponse = await request.post('/graphql', {
      headers: { origin, 'content-type': 'application/json' },
      data: postBody,
    });
    expect(validOriginResponse.status()).toBe(200);

    for (const invalidOrigin of [undefined, 'null', 'not-an-origin', 'http://untrusted.invalid']) {
      const headers: Record<string, string> = { 'content-type': 'application/json' };
      if (invalidOrigin !== undefined) headers['origin'] = invalidOrigin;
      const response = await request.post('/graphql', { headers, data: postBody });
      expect(response.status(), `Origin ${String(invalidOrigin)} must be rejected`).toBeGreaterThanOrEqual(400);
    }

    const rejectedName = `Missing CSRF ${Date.now()}`;
    const rejectedMutation = await submitGraphQLWithoutCsrf(page, rejectedName);
    expect(rejectedMutation.errors?.length).toBeGreaterThan(0);
    expect(JSON.stringify(rejectedMutation)).toMatch(/CSRF/i);

    await page.getByTestId('shell-nav-organizations').click();
    await expect(page.getByRole('heading', { name: 'Organizations' })).toBeVisible();
    await expect(page.locator('#organizations-list')).not.toContainText(rejectedName);

    const createdName = `CSRF Protected ${Date.now()}`;
    await page.locator('#org-name-input').fill(createdName);
    await page.locator('#org-create-submit').click();
    await expect(page.locator('#organizations-list')).toContainText(createdName);
    await Promise.all(responseTasks);
    await Promise.all(requestHeaderTasks);
    expect(graphqlRequestHeaders.some((headers) => !!headers['x-csrf-token'])).toBe(true);
    expect(graphqlRequestHeaders.some((headers) => headers['cookie']?.includes('isched_session='))).toBe(true);
    expect(graphqlRequestHeaders.every((headers) => !headers['authorization'])).toBe(true);
    expect(graphqlRequestUrls.every((url) => !new URL(url).search.includes('token='))).toBe(true);

    const browserStorage = await page.evaluate(async () => {
      const jwtLike = /eyJ[A-Za-z0-9_-]{5,}\.[A-Za-z0-9_-]{5,}\.[A-Za-z0-9_-]*/;
      const hits: string[] = [];
      for (const storage of [localStorage, sessionStorage]) {
        for (let index = 0; index < storage.length; index += 1) {
          const key = storage.key(index) ?? '';
          const value = storage.getItem(key) ?? '';
          if (jwtLike.test(value) || /token|jwt/i.test(key)) hits.push(key);
        }
      }
      const factory = indexedDB as IDBFactory & { databases?: () => Promise<IDBDatabaseInfo[]> };
      const databases = factory.databases ? await factory.databases() : [];
      for (const database of databases) {
        if (/auth|token|jwt|session/i.test(database.name ?? '')) hits.push(database.name ?? '');
      }
      return hits;
    });
    expect(browserStorage).toEqual([]);
  });
});
