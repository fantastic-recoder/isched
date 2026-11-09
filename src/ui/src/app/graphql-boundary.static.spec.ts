// SPDX-License-Identifier: MPL-2.0
/**
 * @file graphql-boundary.static.spec.ts
 * @brief Static guard (spec 013, FR transport boundary): application source may talk to the
 *        backend only via `/graphql` — no fetch/XHR, no REST paths, no hard-coded hosts.
 */

import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';

function sourceFiles(dir: string): string[] {
  return readdirSync(dir).flatMap((name) => {
    const full = join(dir, name);
    if (statSync(full).isDirectory()) return sourceFiles(full);
    return full.endsWith('.ts') && !full.endsWith('.spec.ts') ? [full] : [];
  });
}

const files = sourceFiles(__dirname).map((path) => ({ path, text: readFileSync(path, 'utf8') }));

describe('GraphQL-only transport boundary', () => {
  it('scans application sources', () => {
    expect(files.length).toBeGreaterThan(10);
  });

  it('does not use fetch or XMLHttpRequest', () => {
    const offenders = files.filter((f) => /\bfetch\s*\(|XMLHttpRequest/.test(f.text)).map((f) => f.path);
    expect(offenders).toEqual([]);
  });

  it('HttpClient calls only target the /graphql endpoint', () => {
    for (const f of files) {
      const literalCalls = [...f.text.matchAll(/\.(?:get|post|put|patch|delete|request)\s*(?:<[^()]*>)?\s*\(\s*['"`]([^'"`]*)['"`]/g)];
      const bad = literalCalls
        .map((m) => m[1])
        .filter((url) => url.startsWith('/') || /^https?:/.test(url))
        .filter((url) => url !== '/graphql');
      expect({ file: f.path, bad }).toEqual({ file: f.path, bad: [] });
    }
  });

  it('endpoint constants are /graphql', () => {
    for (const f of files) {
      for (const m of f.text.matchAll(/\bendpoint\s*=\s*['"`]([^'"`]*)['"`]/g)) {
        expect({ file: f.path, endpoint: m[1] }).toEqual({ file: f.path, endpoint: '/graphql' });
      }
    }
  });

  it('has no hard-coded backend hosts or REST-style api paths', () => {
    const offenders = files
      .filter((f) => /['"`](?:https?|wss?):\/\/(?!www\.w3\.org)[^'"`$]/.test(f.text) || /['"`]\/api\//.test(f.text))
      .map((f) => f.path);
    expect(offenders).toEqual([]);
  });

  it('uses only same-origin GraphQL WebSockets with credential-free initialization', () => {
    const socketSources = files.filter((f) => /\bnew WebSocket\s*\(/.test(f.text));
    expect(socketSources).toHaveLength(1);
    for (const source of socketSources) {
      expect(source.text).toContain('window.location.host}/graphql');
      expect(source.text).toContain("JSON.stringify({ type: 'connection_init' })");
      expect(source.text).not.toMatch(/Authorization|Bearer|accessToken|access_token|jwt/i);
    }
  });

  it('does not define bearer or JWT state in application sources', () => {
    const offenders = files
      .filter((f) => /\b(?:bearerToken|accessToken|access_token|jwtToken)\b/i.test(f.text))
      .map((f) => f.path);
    expect(offenders).toEqual([]);
  });
});
