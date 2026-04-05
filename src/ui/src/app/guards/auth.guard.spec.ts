// SPDX-License-Identifier: MPL-2.0
/**
 * @file auth.guard.spec.ts
 * @brief Unit tests for authGuard (T-UI-F-014)
 */

import { TestBed } from '@angular/core/testing';
import { provideRouter, ActivatedRouteSnapshot, RouterStateSnapshot, UrlTree } from '@angular/router';
import { of, throwError } from 'rxjs';
import { authGuard } from './auth.guard';
import { AuthService } from '../services/auth.service';
import { BootstrapService } from '../services/bootstrap.service';
import { SessionBootstrapStateService } from '../services/session-bootstrap-state.service';

// Dummy components for router outlets
import { Component } from '@angular/core';
@Component({ template: '', standalone: true })
class DummyComponent {}

describe('authGuard', () => {
  let auth: AuthService;
  let bootstrap: BootstrapService;
  let sessionBootstrapState: SessionBootstrapStateService;

  const dummyRoute = {} as ActivatedRouteSnapshot;
  const dummyState = {} as RouterStateSnapshot;

  function runGuard() {
    return TestBed.runInInjectionContext(() => authGuard(dummyRoute, dummyState));
  }

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: 'bootstrap', component: DummyComponent },
          { path: 'login',     component: DummyComponent },
          { path: 'dashboard', component: DummyComponent, canActivate: [authGuard] },
        ]),
      ],
    }).compileComponents();

    auth     = TestBed.inject(AuthService);
    bootstrap = TestBed.inject(BootstrapService);
    sessionBootstrapState = TestBed.inject(SessionBootstrapStateService);
  });

  it('redirects to /bootstrap when seedModeActive is true', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: true } }),
    );

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/bootstrap');
      expect(bootstrap.bootstrapStatus).toHaveBeenCalledWith('GuardProbe');
      done();
    });
  });

  it('revalidates once and redirects to /login when session is invalid', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    const bootstrapSessionSpy = jest.spyOn(auth, 'bootstrapSession').mockReturnValue(of(false));

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/login');
      expect(bootstrapSessionSpy).toHaveBeenCalledTimes(1);
      expect(sessionBootstrapState.sessionBootstrapState().firstGuardRevalidationComplete).toBe(true);
      expect(sessionBootstrapState.sessionBootstrapState().initialRouteResolved).toBe(true);
      done();
    });
  });

  it('returns true when session revalidation succeeds', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    const bootstrapSessionSpy = jest.spyOn(auth, 'bootstrapSession').mockReturnValue(of(true));

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBe(true);
      expect(bootstrapSessionSpy).toHaveBeenCalledTimes(1);
      expect(sessionBootstrapState.sessionBootstrapState().sessionAuthenticated).toBe(true);
      done();
    });
  });

  it('does not revalidate again after the first guarded pass', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    sessionBootstrapState.markFirstGuardRevalidationComplete();
    sessionBootstrapState.markSessionKnown(true);
    const bootstrapSessionSpy = jest.spyOn(auth, 'bootstrapSession').mockReturnValue(of(true));
    jest.spyOn(auth, 'isLoggedIn').mockReturnValue(true);

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBe(true);
      expect(bootstrapSessionSpy).not.toHaveBeenCalled();
      done();
    });
  });

  it('redirects to /login on later guarded navigations after an invalid first revalidation without revalidating again', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    const bootstrapSessionSpy = jest.spyOn(auth, 'bootstrapSession').mockReturnValue(of(false));

    const firstResult$ = runGuard() as ReturnType<typeof runGuard>;
    (firstResult$ as ReturnType<typeof of>).subscribe((firstResult: boolean | UrlTree) => {
      expect((firstResult as UrlTree).toString()).toBe('/login');
      expect(bootstrapSessionSpy).toHaveBeenCalledTimes(1);

      jest.spyOn(auth, 'isLoggedIn').mockReturnValue(false);

      const secondResult$ = runGuard() as ReturnType<typeof runGuard>;
      (secondResult$ as ReturnType<typeof of>).subscribe((secondResult: boolean | UrlTree) => {
        expect(secondResult).toBeInstanceOf(UrlTree);
        expect((secondResult as UrlTree).toString()).toBe('/login');
        expect(bootstrapSessionSpy).toHaveBeenCalledTimes(1);
        expect(sessionBootstrapState.sessionBootstrapState().firstGuardRevalidationComplete).toBe(true);
        done();
      });
    });
  });

  it('redirects to /login and marks revalidation complete when the first revalidation errors', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    const bootstrapSessionSpy = jest.spyOn(auth, 'bootstrapSession').mockReturnValue(
      throwError(() => new Error('session probe failed')),
    );

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/login');
      expect(bootstrapSessionSpy).toHaveBeenCalledTimes(1);
      expect(sessionBootstrapState.sessionBootstrapState().firstGuardRevalidationComplete).toBe(true);
      expect(sessionBootstrapState.sessionBootstrapState().sessionAuthenticated).toBe(false);
      done();
    });
  });

  it('redirects to /login when bootstrap status probe fails', (done) => {
    jest.spyOn(bootstrap, 'bootstrapStatus').mockReturnValue(
      throwError(() => new Error('Network error')),
    );

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/login');
      expect(sessionBootstrapState.sessionBootstrapState().initialRouteResolved).toBe(true);
      done();
    });
  });
});
