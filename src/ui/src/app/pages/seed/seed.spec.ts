// SPDX-License-Identifier: MPL-2.0
/**
 * @file seed.spec.ts
 * @brief Unit tests for SeedComponent (T-UI-F-012)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter, Router } from '@angular/router';
import { SeedComponent } from './seed';

describe('SeedComponent', () => {
  let httpMock: HttpTestingController;
  let router: Router;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SeedComponent],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        provideRouter([{ path: 'login', component: SeedComponent }]),
      ],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
    router = TestBed.inject(Router);
  });

  afterEach(() => httpMock.verify());

  function createFixture() {
    const fixture = TestBed.createComponent(SeedComponent);
    fixture.detectChanges();
    return fixture;
  }

  it('form is invalid if password is shorter than 12 characters', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    comp.form.setValue({ email: 'a@b.com', password: 'short', confirmPassword: 'short' });
    expect(comp.form.invalid).toBe(true);
    expect(comp.password.hasError('minlength')).toBe(true);
  });

  it('form is invalid if passwords do not match', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    comp.form.setValue({
      email: 'a@b.com',
      password: 'LongEnough1234',
      confirmPassword: 'DifferentPass1',
    });
    expect(comp.form.hasError('passwordMismatch')).toBe(true);
  });

  it('does not POST when form is invalid', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    // Leave form empty (invalid)
    comp.onSubmit();
    httpMock.expectNone('/graphql');
  });

  it('navigates to /login on successful mutation', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);

    comp.form.setValue({
      email: 'admin@x.com',
      password: 'LongEnough1234',
      confirmPassword: 'LongEnough1234',
    });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush({
      data: { createPlatformAdmin: { id: '1', email: 'admin@x.com' } },
    });

    // Wait for microtasks (subscribe next callback)
    Promise.resolve().then(() => {
      expect(navSpy).toHaveBeenCalledWith(['/login']);
      done();
    });
  });

  it('shows error banner on server error', (done) => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;

    comp.form.setValue({
      email: 'admin@x.com',
      password: 'LongEnough1234',
      confirmPassword: 'LongEnough1234',
    });
    comp.onSubmit();

    httpMock.expectOne('/graphql').flush(
      { errors: [{ message: 'Seed mode not active' }] },
    );

    Promise.resolve().then(() => {
      expect(comp.errorMsg()).toBe('Seed mode not active');
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

  it('submit button is disabled while pending', () => {
    const fixture = createFixture();
    const comp = fixture.componentInstance;
    comp.pending.set(true);
    fixture.detectChanges();
    const btn = (fixture.nativeElement as HTMLElement).querySelector<HTMLButtonElement>('[type="submit"]');
    expect(btn?.disabled).toBe(true);
  });
});

