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
  });

  function createFixture() {
    const fixture = TestBed.createComponent(LoginComponent);
    fixture.detectChanges();
    return fixture;
  }

  it('bootstraps session and navigates to /dashboard on success', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);

    comp.form.setValue({ email: 'admin@x.com', password: 'somepassword' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush({
      data: { login: { token: 'tok_xyz', expiresAt: '2099-01-01T00:00:00Z' } },
    });
    httpMock.expectOne('/graphql').flush({ data: { currentUser: { id: 'u1' } } });

    Promise.resolve().then(() => {
      expect(navSpy).toHaveBeenCalledWith(['/dashboard']);
      done();
    });
  });

  it('shows generic auth alert on non-lockout sign-in failure', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;

    comp.form.setValue({ email: 'admin@x.com', password: 'wrong' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush(
      { errors: [{ message: 'Invalid credentials' }] },
    );

    Promise.resolve().then(() => {
      expect(comp.authAlert()?.category).toBe('AuthFailure');
      expect(comp.authAlert()?.body).toBe('Invalid credentials');
      fixture.detectChanges();
      const compiled = fixture.nativeElement as HTMLElement;
      expect(compiled.querySelector('.alert-error')).toBeTruthy();
      done();
    });
  });

  it('shows deterministic lockout guidance with retry metadata', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;

    comp.form.setValue({ email: 'admin@x.com', password: 'wrong' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush({
      errors: [{
        message: 'rate limited',
        extensions: { code: 'RATE_LIMITED', retryAfterMs: 15000 },
      }],
    });

    Promise.resolve().then(() => {
      expect(comp.lockoutAlert()?.category).toBe('AuthRateLimited');
      expect(comp.lockoutAlert()?.body).toContain('about 15 seconds');
      fixture.detectChanges();
      const compiled = fixture.nativeElement as HTMLElement;
      expect(compiled.querySelector('.alert-warning')?.textContent).toContain('Retry in about 15 seconds');
      done();
    });
  });

  it('shows fallback lockout guidance when retry metadata is absent', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;

    comp.form.setValue({ email: 'admin@x.com', password: 'wrong' });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush({
      errors: [{
        message: 'rate limited',
        extensions: { code: 'RATE_LIMITED' },
      }],
    });

    Promise.resolve().then(() => {
      expect(comp.lockoutAlert()?.category).toBe('AuthRateLimited');
      expect(comp.lockoutAlert()?.body).toBe(
        'Too many failed sign-in attempts. Please wait a few minutes before trying again.',
      );
      fixture.detectChanges();
      const compiled = fixture.nativeElement as HTMLElement;
      expect(compiled.querySelector('.alert-warning')?.textContent).toContain('Please wait a few minutes');
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

  it('provides accessible labels and required semantics for interactive login controls', () => {
    const fixture = createFixture();
    const root = fixture.nativeElement as HTMLElement;

    const form = root.querySelector('form');
    const email = root.querySelector<HTMLInputElement>('#email');
    const password = root.querySelector<HTMLInputElement>('#password');
    const toggle = root.querySelector<HTMLButtonElement>('#login-password-toggle');
    const submit = root.querySelector<HTMLButtonElement>('#login-submit');

    expect(form?.getAttribute('aria-label')).toBe('Sign in form');
    expect(root.querySelector('label[for="email"]')?.textContent).toContain('Email');
    expect(root.querySelector('label[for="password"]')?.textContent).toContain('Password');
    expect(email?.getAttribute('aria-required')).toBe('true');
    expect(password?.getAttribute('aria-required')).toBe('true');
    expect(email?.getAttribute('aria-describedby')).toContain('login-email-help');
    expect(password?.getAttribute('aria-describedby')).toContain('login-password-help');
    expect(toggle?.getAttribute('aria-label')).toBe('Show password');
    expect(submit?.getAttribute('aria-label')).toBe('Sign in to isched');
    expect(submit?.className).toContain('focus-visible:outline');
  });

  it('keeps keyboard-only tab order aligned with visual order', () => {
    const fixture = createFixture();
    const root = fixture.nativeElement as HTMLElement;

    const focusables = Array.from(
      root.querySelectorAll<HTMLElement>('#email, #password, #login-password-toggle, #login-submit'),
    );

    expect(focusables.map((el) => el.id)).toEqual([
      'email',
      'password',
      'login-password-toggle',
      'login-submit',
    ]);
    expect(focusables.every((el) => el.tabIndex >= 0)).toBe(true);
  });
});

