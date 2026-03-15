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
    sessionStorage.clear();
  });

  afterEach(() => {
    httpMock.verify();
    sessionStorage.clear();
  });

  it('setToken / getToken round-trip uses "isched_token" key', () => {
    service.setToken('tok_abc');
    expect(sessionStorage.getItem('isched_token')).toBe('tok_abc');
    expect(service.getToken()).toBe('tok_abc');
  });

  it('isLoggedIn() returns true when token is set', () => {
    service.setToken('tok_abc');
    expect(service.isLoggedIn()).toBe(true);
  });

  it('isLoggedIn() returns false when no token', () => {
    expect(service.isLoggedIn()).toBe(false);
  });

  it('clearToken() removes token from sessionStorage', () => {
    service.setToken('tok_abc');
    service.clearToken();
    expect(service.getToken()).toBeNull();
    expect(service.isLoggedIn()).toBe(false);
  });

  it('logout() calls clearToken and fires logout mutation', () => {
    service.setToken('tok_abc');
    service.logout();
    expect(service.isLoggedIn()).toBe(false);
    // Drain the logout mutation request
    httpMock.expectOne('/graphql').flush({ data: {} });
  });
});
