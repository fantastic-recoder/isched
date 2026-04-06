import { expect, test } from '@playwright/test';
import { spawn, type ChildProcess } from 'node:child_process';

const DEV_SERVER_URL = 'http://127.0.0.1:4200';

test.describe.configure({ timeout: 120_000 });

async function waitForDevServerReady(url: string, timeoutMs = 90_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) {
        return;
      }
    } catch {
      // Dev server is not ready yet.
    }

    await new Promise((resolveDelay) => setTimeout(resolveDelay, 500));
  }

  throw new Error(`Angular dev server did not become ready at ${url} within ${timeoutMs}ms`);
}

function startAngularDevServer(): ChildProcess {
  const child = spawn('pnpm', ['exec', 'ng', 'serve', '--host', '127.0.0.1', '--port', '4200'], {
    cwd: __dirname + '/..',
    stdio: ['ignore', 'pipe', 'pipe'],
    env: {
      ...process.env,
      CI: '1',
    },
  });

  child.stdout?.on('data', (chunk: Buffer) => {
    const text = chunk.toString().trimEnd();
    if (text.length > 0) {
      process.stdout.write(`[ng-dev] ${text}\n`);
    }
  });

  child.stderr?.on('data', (chunk: Buffer) => {
    const text = chunk.toString().trimEnd();
    if (text.length > 0) {
      process.stderr.write(`[ng-dev] ${text}\n`);
    }
  });

  return child;
}

test.describe('Angular dev-server proxy', () => {
  let devServer: ChildProcess | null = null;

  test.beforeAll(async () => {
    test.setTimeout(120_000);
    devServer = startAngularDevServer();
    await waitForDevServerReady(`${DEV_SERVER_URL}/dev/proxy`);
  });

  test.afterAll(async () => {
    if (!devServer?.pid) {
      return;
    }

    devServer.kill('SIGTERM');
    await new Promise((resolveDone) => setTimeout(resolveDone, 1000));
  });

  test('proxies /graphql requests to backend when running via Angular dev server', async ({ page }) => {
    test.setTimeout(120_000);

    await page.goto(`${DEV_SERVER_URL}/`);

    const result = await page.evaluate(async () => {
      const response = await fetch('/graphql', {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
        },
        body: JSON.stringify({
          query: 'query BootstrapStatus { systemState { seedModeActive } }',
        }),
      });

      return {
        ok: response.ok,
        status: response.status,
        body: await response.text(),
      };
    });

    expect(result.ok).toBe(true);
    expect(result.status).toBe(200);
    expect(result.body).toContain('seedModeActive');
  });
});
