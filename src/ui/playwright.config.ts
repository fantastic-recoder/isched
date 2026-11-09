import { defineConfig } from '@playwright/test';

const SERVER_PORT = Number(process.env['ISCHED_SERVER_PORT'] ?? 18080);

export default defineConfig({
  testDir: './e2e',
  timeout: 30_000,
  fullyParallel: false,
  retries: 0,
  workers: 1,
  reporter: [['list']],
  use: {
    // Secure cookies are accepted by Chromium on localhost; do not use 127.0.0.1
    // for the cookie-only browser acceptance origin.
    baseURL: `http://localhost:${SERVER_PORT}`,
    headless: true,
    trace: 'on-first-retry',
  },
  globalSetup: './e2e/global-setup.ts',
  globalTeardown: './e2e/global-teardown.ts',
  projects: [
    {
      name: 'chromium',
      use: { browserName: 'chromium' },
    },
  ],
});
