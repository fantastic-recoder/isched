import type { Config } from 'jest';

// Only specify deltas on top of @angular-builders/jest default config.
// The builder (zoneless: true) handles all Angular testing infrastructure automatically.
const config: Config = {
  coverageDirectory: 'coverage',
  collectCoverageFrom: ['src/**/*.ts', '!src/**/*.spec.ts'],
};

export default config;
