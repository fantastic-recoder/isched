import { existsSync, readFileSync, rmSync } from 'node:fs';
import { resolve } from 'node:path';

type E2EState = {
  pid: number;
  dataDir: string;
  port: number;
};

const STATE_FILE = resolve(__dirname, '.server-state.json');

export default async function globalTeardown(): Promise<void> {
  if (!existsSync(STATE_FILE)) {
    return;
  }

  const state = JSON.parse(readFileSync(STATE_FILE, 'utf-8')) as E2EState;

  // pid === 0 means the server is managed externally (e.g. by run_e2e.py).
  // Leave both the process and the data directory alone.
  if (state.pid !== 0) {
    try {
      process.kill(state.pid, 'SIGTERM');
    } catch {
      // Process may already be gone.
    }

    rmSync(state.dataDir, { recursive: true, force: true });
  }

  rmSync(STATE_FILE, { force: true });
}

