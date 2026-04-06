import { mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';

type E2EState = {
  pid: number;
  dataDir: string;
  port: number;
};

export const SERVER_PORT = 18080;
export const APP_ENTRY_PATH = '/isched';
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

/** Forward a chunk of server output to the given stream, prefixing every
 *  non-empty line with `[isched]` so server logs are visually distinct from
 *  Playwright's own output. */
function forwardServerOutput(stream: NodeJS.WriteStream, chunk: Buffer): void {
  const text = chunk.toString();
  const lines = text.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    // Skip the trailing empty string produced by a final newline.
    if (line === '' && i === lines.length - 1) continue;
    stream.write(`[isched] ${line}\n`);
  }
}

export default async function globalSetup(): Promise<void> {
  const port = Number(process.env['ISCHED_SERVER_PORT'] ?? SERVER_PORT);

  // When run_e2e.py (or another external harness) manages the server it sets
  // ISCHED_EXTERNAL_SERVER=1.  In that case we skip launching a new process
  // and only wait until the already-running server is reachable.
  if (process.env['ISCHED_EXTERNAL_SERVER'] === '1') {
    console.log(`[e2e] ISCHED_EXTERNAL_SERVER=1 — skipping server launch, waiting for http://127.0.0.1:${port}${APP_ENTRY_PATH}`);
    await waitForServerReady(`http://127.0.0.1:${port}${APP_ENTRY_PATH}`);
    // Write a sentinel state so teardown knows not to touch the process.
    const state: E2EState = { pid: 0, dataDir: '', port };
    writeFileSync(STATE_FILE, JSON.stringify(state), 'utf-8');
    return;
  }

  const repoRoot = resolve(__dirname, '../../..');

  // ISCHED_BUILD_DIR selects which CMake build tree to use.
  // Defaults to the standard debug build directory.
  const buildDir = process.env['ISCHED_BUILD_DIR'] ?? 'cmake-build-debug';
  const serverBinary = resolve(repoRoot, buildDir, 'src/main/cpp/isched/isched_srv');

  const parentDir = join(tmpdir(), 'isched-playwright');
  mkdirSync(parentDir, { recursive: true });
  const dataDir = mkdtempSync(join(parentDir, 'data-'));

  console.log(`[e2e] CMake build dir        : ${buildDir}`);
  console.log(`[e2e] Starting server binary : ${serverBinary}`);
  console.log(`[e2e] Data directory         : ${dataDir}`);
  console.log(`[e2e] Listening on port      : ${port}`);

  const child = spawn(serverBinary, ['--data-dir', dataDir], {
    cwd: repoRoot,
    // Pipe stdout/stderr so we can prefix each line with [isched].
    stdio: ['ignore', 'pipe', 'pipe'],
    env: {
      ...process.env,
      ISCHED_SERVER_HOST: '127.0.0.1',
      ISCHED_SERVER_PORT: String(port),
    },
  });

  child.stdout?.on('data', (chunk: Buffer) => forwardServerOutput(process.stdout, chunk));
  child.stderr?.on('data', (chunk: Buffer) => forwardServerOutput(process.stderr, chunk));

  child.on('error', (err) => {
    console.error(`[e2e] Server process error: ${err.message}`);
  });
  child.on('exit', (code, signal) => {
    if (code !== null) console.log(`[e2e] Server exited with code ${code}`);
    if (signal !== null) console.log(`[e2e] Server killed by signal ${signal}`);
  });

  if (!child.pid) {
    throw new Error(`Failed to start isched_srv — binary not found or not executable: ${serverBinary}`);
  }

  console.log(`[e2e] Server process started (pid ${child.pid})`);

  const state: E2EState = {
    pid: child.pid,
    dataDir,
    port,
  };
  writeFileSync(STATE_FILE, JSON.stringify(state), 'utf-8');

  await waitForServerReady(`http://127.0.0.1:${port}${APP_ENTRY_PATH}`);
  console.log(`[e2e] Server is ready — running tests`);
}

