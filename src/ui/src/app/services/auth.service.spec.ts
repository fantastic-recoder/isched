// SPDX-License-Identifier: MPL-2.0
/**
 * @file auth.service.spec.ts
 * @brief Unit tests for AuthService (T-UI-F-011)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { AuthService } from './auth.service';

describe('AuthService', () => {
  let service: AuthService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [AuthService, provideHttpClient(), provideHttpClientTesting()],
    });
    service = TestBed.inject(AuthService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('bootstrapSession marks authenticated when currentUser exists', (done) => {
    service.bootstrapSession().subscribe((isLoggedIn) => {
      expect(isLoggedIn).toBe(true);
      expect(service.isLoggedIn()).toBe(true);
      expect(service.getCsrfToken()).toMatch(/^csrf_/);
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { currentUser: { id: 'u1' } } });
  });

  it('signIn authenticates through bootstrapSession without exposing token storage', (done) => {
    service.signIn('admin@example.com', 'password').subscribe((ok) => {
      expect(ok).toBe(true);
      expect(service.isLoggedIn()).toBe(true);
      expect(service.getCsrfToken()).toMatch(/^csrf_/);
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { login: { token: 'opaque-cookie-token' } } });
    httpMock.expectOne('/graphql').flush({ data: { currentUser: { id: 'u1' } } });
  });

  it('isLoggedIn() returns false by default', () => {
    expect(service.isLoggedIn()).toBe(false);
  });

  it('tracks CSRF token in memory', () => {
    service.setCsrfToken('csrf_123');
    expect(service.getCsrfToken()).toBe('csrf_123');
  });

  it('signOut clears local auth state after mutation', (done) => {
    service.setCsrfToken('csrf_123');
    service.signOut().subscribe((ok) => {
      expect(ok).toBe(true);
      expect(service.isLoggedIn()).toBe(false);
      expect(service.getCsrfToken()).toBeNull();
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { logout: true } });
  });

  it('does not write auth state to localStorage/sessionStorage during bootstrap', (done) => {
    const localSetSpy = jest.spyOn(Object.getPrototypeOf(window.localStorage), 'setItem');
    const sessionSetSpy = jest.spyOn(Object.getPrototypeOf(window.sessionStorage), 'setItem');

    service.bootstrapSession().subscribe(() => {
      expect(localSetSpy).not.toHaveBeenCalled();
      expect(sessionSetSpy).not.toHaveBeenCalled();
      localSetSpy.mockRestore();
      sessionSetSpy.mockRestore();
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { currentUser: { id: 'u1' } } });
  });

  it('does not write auth state to localStorage/sessionStorage during signOut', (done) => {
    const localSetSpy = jest.spyOn(Object.getPrototypeOf(window.localStorage), 'setItem');
    const sessionSetSpy = jest.spyOn(Object.getPrototypeOf(window.sessionStorage), 'setItem');

    service.signOut().subscribe(() => {
      expect(localSetSpy).not.toHaveBeenCalled();
      expect(sessionSetSpy).not.toHaveBeenCalled();
      localSetSpy.mockRestore();
      sessionSetSpy.mockRestore();
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { logout: true } });
  });

  it('surfaces deterministic lockout guidance with retry timing when backend provides retryAfterMs', (done) => {
    service.signIn('admin@example.com', 'wrong-password').subscribe({
      next: () => done.fail('expected lockout error'),
      error: (err: Error) => {
        expect(err.name).toBe('AuthSignInError');
        expect(err.message).toContain('Too many failed sign-in attempts');
        expect(err.message).toContain('about 42 seconds');
        const signInError = err as Error & {
          alert: { category: string };
          outcome: { status: string; retryAfterMs?: number };
        };
        expect(signInError.alert.category).toBe('AuthRateLimited');
        expect(signInError.outcome.status).toBe('RateLimited');
        expect(signInError.outcome.retryAfterMs).toBe(42000);
        done();
      },
    });

    httpMock.expectOne('/graphql').flush({
      errors: [
        {
          message: 'Rate limited',
          extensions: {
            code: 'RATE_LIMITED',
            retryAfterMs: 42000,
          },
        },
      ],
    });
  });

  it('uses fallback lockout guidance when retryAfterMs metadata is absent', (done) => {
    service.signIn('admin@example.com', 'wrong-password').subscribe({
      next: () => done.fail('expected lockout error'),
      error: (err: Error) => {
        expect(err.name).toBe('AuthSignInError');
        expect(err.message).toBe('Too many failed sign-in attempts. Please wait a few minutes before trying again.');
        const signInError = err as Error & {
          alert: { category: string };
          outcome: { status: string; retryAfterMs?: number };
        };
        expect(signInError.alert.category).toBe('AuthRateLimited');
        expect(signInError.outcome.status).toBe('RateLimited');
        expect(signInError.outcome.retryAfterMs).toBeUndefined();
        done();
      },
    });

    httpMock.expectOne('/graphql').flush({
      errors: [
        {
          message: 'Rate limited',
          extensions: {
            code: 'RATE_LIMITED',
          },
        },
      ],
    });
  });
});
