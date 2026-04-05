import type { Config } from 'jest';

// Only specify deltas on top of @angular-builders/jest default config.
// The builder (zoneless: true) handles Angular testing environment setup.
const config: Config = {
  coverageDirectory: 'coverage',
  collectCoverageFrom: ['src/**/*.ts', '!src/**/*.spec.ts'],
  testMatch: ['<rootDir>/src/**/*.spec.ts'],
  testPathIgnorePatterns: ['<rootDir>/e2e/'],
};

export default config;
