import { mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';

type E2EState = {
  pid: number;
  dataDir: string;
  port: number;
};

const SERVER_PORT = 18080;
const STATE_FILE = resolve(__dirname, '.server-state.json');

async function waitForServerReady(url: string, timeoutMs = 20_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) {
        return;
      }
    } catch {
      // Server not ready yet.
    }
    await new Promise((resolveDelay) => setTimeout(resolveDelay, 250));
  }

  throw new Error(`Server did not become ready at ${url} within ${timeoutMs}ms`);
}

export default async function globalSetup(): Promise<void> {
  const port = Number(process.env['ISCHED_SERVER_PORT'] ?? SERVER_PORT);

  // When run_e2e.py (or another external harness) manages the server it sets
  // ISCHED_EXTERNAL_SERVER=1.  In that case we skip launching a new process
  // and only wait until the already-running server is reachable.
  if (process.env['ISCHED_EXTERNAL_SERVER'] === '1') {
    await waitForServerReady(`http://127.0.0.1:${port}/graphql`);
    // Write a sentinel state so teardown knows not to touch the process.
    const state: E2EState = { pid: 0, dataDir: '', port };
    writeFileSync(STATE_FILE, JSON.stringify(state), 'utf-8');
    return;
  }

  const repoRoot = resolve(__dirname, '../../..');
  const serverBinary = resolve(repoRoot, 'cmake-build-debug/src/main/cpp/isched/isched_srv');

  const parentDir = join(tmpdir(), 'isched-playwright');
  mkdirSync(parentDir, { recursive: true });
  const dataDir = mkdtempSync(join(parentDir, 'data-'));

  const child = spawn(serverBinary, ['--data-dir', dataDir], {
    cwd: repoRoot,
    stdio: 'inherit',
    env: {
      ...process.env,
      ISCHED_SERVER_HOST: '127.0.0.1',
      ISCHED_SERVER_PORT: String(port),
    },
  });

  if (!child.pid) {
    throw new Error('Failed to start isched_srv process for Playwright tests');
  }

  const state: E2EState = {
    pid: child.pid,
    dataDir,
    port,
  };
  writeFileSync(STATE_FILE, JSON.stringify(state), 'utf-8');

  await waitForServerReady(`http://127.0.0.1:${port}/graphql`);
}

