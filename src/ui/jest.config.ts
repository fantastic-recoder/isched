import type { Config } from 'jest';

// Only specify deltas on top of @angular-builders/jest default config.
// The builder (zoneless: true) handles Angular testing environment setup.
const config: Config = {
  coverageDirectory: 'coverage',
  collectCoverageFrom: ['src/**/*.ts', '!src/**/*.spec.ts'],
  testMatch: ['<rootDir>/src/**/*.spec.ts'],
  testPathIgnorePatterns: ['<rootDir>/e2e/'],
  // Mock CodeMirror 6 and cm6-graphql — they ship ESM-only and are not needed in unit tests.
  // The QueryEditorComponent is tested via its public API, not its internal CM6 state.
  moduleNameMapper: {
    '^codemirror$': '<rootDir>/src/__mocks__/codemirror.ts',
    '^@codemirror/(.*)$': '<rootDir>/src/__mocks__/codemirror.ts',
    '^cm6-graphql$': '<rootDir>/src/__mocks__/codemirror.ts',
  },
};

export default config;
