import { expect, test } from '@playwright/test';
import { spawn, type ChildProcess } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import {
  ADMIN_DISPLAY_NAME,
  ADMIN_EMAIL,
  ADMIN_PASSWORD,
} from './authenticated-session.helpers';

const DEV_SERVER_URL = 'http://localhost:4200';
const BACKEND_PORT = Number(process.env['ISCHED_SERVER_PORT'] ?? '8080');

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

function startAngularDevServer(proxyConfigPath: string): ChildProcess {
  const child = spawn('pnpm', [
    'exec',
    'ng',
    'serve',
    '--host',
    '127.0.0.1',
    '--port',
    '4200',
    '--proxy-config',
    proxyConfigPath,
  ], {
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
  let proxyConfigDir: string | null = null;

  test.beforeAll(async () => {
    test.setTimeout(120_000);
    proxyConfigDir = mkdtempSync(join(tmpdir(), 'isched-dev-proxy-'));
    const proxyConfigPath = join(proxyConfigDir, 'proxy.conf.json');
    writeFileSync(
      proxyConfigPath,
      JSON.stringify(
        {
          '/graphql': {
            target: `http://127.0.0.1:${BACKEND_PORT}`,
            secure: false,
            changeOrigin: true,
            xfwd: true,
            ws: true,
          },
        },
        null,
        2,
      ),
      'utf8',
    );

    devServer = startAngularDevServer(proxyConfigPath);
    await waitForDevServerReady(`${DEV_SERVER_URL}/dev/proxy`);
  });

  test.afterAll(async () => {
    if (!devServer?.pid) {
      return;
    }

    devServer.kill('SIGTERM');
    await new Promise((resolveDone) => setTimeout(resolveDone, 1000));

    if (proxyConfigDir) {
      rmSync(proxyConfigDir, { recursive: true, force: true });
      proxyConfigDir = null;
    }
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
          query: 'query BootstrapStatus { isched { systemState { seedModeActive } } }',
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

  test('proxies WebSocket upgrades on /graphql (graphql-transport-ws handshake)', async ({ page }) => {
    test.setTimeout(120_000);

    const applicationPaths: string[] = [];
    const graphqlHeaders: Array<Record<string, string>> = [];
    const requestHeaderTasks: Promise<void>[] = [];
    const sentFrames: string[] = [];
    const websocketUrls: string[] = [];
    page.on('request', (request) => {
      if (request.resourceType() === 'xhr' || request.resourceType() === 'fetch') {
        applicationPaths.push(new URL(request.url()).pathname);
        if (new URL(request.url()).pathname === '/graphql') {
          requestHeaderTasks.push(
            request.allHeaders().then((headers) => {
              graphqlHeaders.push(headers);
            }),
          );
        }
      }
    });
    page.on('websocket', (socket) => {
      websocketUrls.push(new URL(socket.url()).pathname);
      socket.on('framesent', (frame) => sentFrames.push(String(frame.payload)));
    });

    await page.goto(`${DEV_SERVER_URL}/isched`);
    await page.waitForURL(/(?:\/isched)?\/(bootstrap|login|dashboard)(?:$|\?)/, { timeout: 15_000 });
    if (page.url().includes('/bootstrap')) {
      await page.locator('#bs-email').fill(ADMIN_EMAIL);
      await page.locator('#bs-displayName').fill(ADMIN_DISPLAY_NAME);
      await page.locator('#bs-password').fill(ADMIN_PASSWORD);
      await page.locator('#bs-submit').click();
      await page.waitForURL(/(?:\/isched)?\/dashboard(?:$|\?)/, { timeout: 15_000 });
    } else if (page.url().includes('/login')) {
      await page.locator('#email').fill(ADMIN_EMAIL);
      await page.locator('#password').fill(ADMIN_PASSWORD);
      await page.locator('#login-submit').click();
      await page.waitForURL(/(?:\/isched)?\/dashboard(?:$|\?)/, { timeout: 15_000 });
    }

    const proxyOrigin = await page.evaluate(() => window.location.origin);
    const cookie = (await page.context().cookies(proxyOrigin)).find(
      (candidate) => candidate.name === 'isched_session',
    );
    expect(cookie).toMatchObject({ secure: true, httpOnly: true, sameSite: 'Strict' });

    const result = await page.evaluate(
      () =>
        new Promise<{ opened: boolean; ack: boolean; protocol: string; closeCode?: number }>((resolve) => {
          const ws = new WebSocket(`ws://${window.location.host}/graphql`, ['graphql-transport-ws']);
          const state = { opened: false, ack: false, protocol: '' };
          const timer = setTimeout(() => {
            ws.close();
            resolve(state);
          }, 10_000);
          ws.onopen = () => {
            state.opened = true;
            state.protocol = ws.protocol;
            ws.send(JSON.stringify({ type: 'connection_init', payload: {} }));
          };
          ws.onmessage = (event) => {
            if (JSON.parse(String(event.data)).type === 'connection_ack') {
              state.ack = true;
              clearTimeout(timer);
              ws.close();
              resolve(state);
            }
          };
          ws.onclose = (event) => {
            clearTimeout(timer);
            resolve({ ...state, closeCode: event.code });
          };
        }),
    );

    expect(result.opened).toBe(true);
    expect(result.ack).toBe(true);
    expect(result.protocol).toBe('graphql-transport-ws');
    await Promise.all(requestHeaderTasks);
    expect(sentFrames).toContain('{"type":"connection_init","payload":{}}');
    expect(sentFrames.join('\n')).not.toMatch(/jwt|token|authorization|bearer/i);
    // Angular's development server can open its own live-reload socket at "/".
    // The application transport itself must use only the GraphQL endpoint.
    expect(websocketUrls.filter((url) => url === '/graphql')).toEqual(['/graphql']);
    expect(applicationPaths.every((path) => path === '/graphql')).toBe(true);
    expect(graphqlHeaders.some((headers) => headers['cookie']?.includes('isched_session='))).toBe(true);
  });
});
