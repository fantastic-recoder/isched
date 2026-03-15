// SPDX-License-Identifier: MPL-2.0
/**
 * @file auth.guard.spec.ts
 * @brief Unit tests for authGuard (T-UI-F-014)
 */

import { TestBed } from '@angular/core/testing';
import { provideRouter, Router, ActivatedRouteSnapshot, RouterStateSnapshot, UrlTree } from '@angular/router';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { of, throwError } from 'rxjs';
import { authGuard } from './auth.guard';
import { GraphQLService } from '../services/graphql.service';
import { AuthService } from '../services/auth.service';

// Dummy components for router outlets
import { Component } from '@angular/core';
@Component({ template: '', standalone: true })
class DummyComponent {}

describe('authGuard', () => {
  let httpMock: HttpTestingController;
  let router: Router;
  let gql: GraphQLService;
  let auth: AuthService;

  const dummyRoute = {} as ActivatedRouteSnapshot;
  const dummyState = {} as RouterStateSnapshot;

  function runGuard() {
    return TestBed.runInInjectionContext(() => authGuard(dummyRoute, dummyState));
  }

  beforeEach(async () => {
    sessionStorage.clear();
    await TestBed.configureTestingModule({
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        provideRouter([
          { path: 'seed',      component: DummyComponent },
          { path: 'login',     component: DummyComponent },
          { path: 'dashboard', component: DummyComponent, canActivate: [authGuard] },
        ]),
      ],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
    router   = TestBed.inject(Router);
    gql      = TestBed.inject(GraphQLService);
    auth     = TestBed.inject(AuthService);
  });

  afterEach(() => {
    httpMock.verify();
    sessionStorage.clear();
  });

  it('redirects to /seed when seedModeActive is true', (done) => {
    jest.spyOn(gql, 'query').mockReturnValue(
      of({ systemState: { seedModeActive: true } }),
    );

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/seed');
      done();
    });
  });

  it('redirects to /login when seedModeActive is false and not logged in', (done) => {
    jest.spyOn(gql, 'query').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    // no token set

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/login');
      done();
    });
  });

  it('returns true when seedModeActive is false and user is logged in', (done) => {
    jest.spyOn(gql, 'query').mockReturnValue(
      of({ systemState: { seedModeActive: false } }),
    );
    auth.setToken('tok_xyz');

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBe(true);
      done();
    });
  });

  it('redirects to /login on network error', (done) => {
    jest.spyOn(gql, 'query').mockReturnValue(
      throwError(() => new Error('Network error')),
    );

    const result$ = runGuard() as ReturnType<typeof runGuard>;
    (result$ as ReturnType<typeof of>).subscribe((result: boolean | UrlTree) => {
      expect(result).toBeInstanceOf(UrlTree);
      expect((result as UrlTree).toString()).toBe('/login');
      done();
    });
  });
});
