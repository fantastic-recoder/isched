// SPDX-License-Identifier: MPL-2.0
/**
 * @file login.spec.ts
 * @brief Unit tests for LoginComponent (T-UI-F-013)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter, Router } from '@angular/router';
import { LoginComponent } from './login';

describe('LoginComponent', () => {
  let httpMock: HttpTestingController;
  let router: Router;

  beforeEach(async () => {
    sessionStorage.clear();
    await TestBed.configureTestingModule({
      imports: [LoginComponent],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        provideRouter([{ path: 'dashboard', component: LoginComponent }]),
      ],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
    router = TestBed.inject(Router);
  });

  afterEach(() => {
    httpMock.verify();
    sessionStorage.clear();
  });

  function createFixture() {
    const fixture = TestBed.createComponent(LoginComponent);
    fixture.detectChanges();
    return fixture;
  }

  it('stores token in sessionStorage and navigates to /dashboard on success', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);

    comp.form.setValue({ email: 'admin@x.com', password: 'somepassword' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush({
      data: { login: { token: 'tok_xyz', expiresAt: '2099-01-01T00:00:00Z' } },
    });

    Promise.resolve().then(() => {
      expect(sessionStorage.getItem('isched_token')).toBe('tok_xyz');
      expect(navSpy).toHaveBeenCalledWith(['/dashboard']);
      done();
    });
  });

  it('shows error banner on server error', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;

    comp.form.setValue({ email: 'admin@x.com', password: 'wrong' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush(
      { errors: [{ message: 'Invalid credentials' }] },
    );

    Promise.resolve().then(() => {
      expect(comp.errorMsg()).toBe('Invalid credentials');
      fixture.detectChanges();
      const compiled = fixture.nativeElement as HTMLElement;
      expect(compiled.querySelector('.alert-error')).toBeTruthy();
      done();
    });
  });

  it('password toggle switches input type', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    expect(comp.showPw()).toBe(false);
    comp.showPw.set(true);
    fixture.detectChanges();
    const pwInput = (fixture.nativeElement as HTMLElement).querySelector<HTMLInputElement>('#password');
    expect(pwInput?.type).toBe('text');
  });

  it('does not POST when form is invalid', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    // empty form
    comp.onSubmit();
    httpMock.expectNone('/graphql');
  });
});

