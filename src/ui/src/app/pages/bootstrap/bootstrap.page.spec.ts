import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter, Router } from '@angular/router';
import { BootstrapPage } from './bootstrap.page';
import { SessionBootstrapStateService } from '../../services/session-bootstrap-state.service';

/** Flush microtask queue so chained observables propagate. */
const flushMicrotasks = () => new Promise<void>((r) => setTimeout(r, 0));

describe('BootstrapPage', () => {
  let httpMock: HttpTestingController;
  let router: Router;
  let sessionBootstrapState: SessionBootstrapStateService;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [BootstrapPage],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        provideRouter([
          { path: 'login', component: BootstrapPage },
          { path: 'dashboard', component: BootstrapPage },
        ]),
      ],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
    router = TestBed.inject(Router);
    sessionBootstrapState = TestBed.inject(SessionBootstrapStateService);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('auto-logs in and navigates to dashboard after successful bootstrap', async () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    // 1) Bootstrap mutation
    httpMock.expectOne('/graphql').flush({
      data: { bootstrapPlatformAdmin: { token: 'x', expiresAt: '2099-01-01T00:00:00Z' } },
    });
    await flushMicrotasks();

    // 2) Auto-login mutation (AuthService.signIn)
    httpMock.expectOne('/graphql').flush({
      data: { login: { token: 'session-jwt' } },
    });
    await flushMicrotasks();

    // 3) Session bootstrap query (AuthService.bootstrapSession → currentUser)
    httpMock.expectOne('/graphql').flush({
      data: { currentUser: { id: '1' } },
    });
    await flushMicrotasks();

    expect(navSpy).toHaveBeenCalledWith(['/dashboard']);
  });

  it('falls back to login page when auto-login fails after bootstrap', async () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    // 1) Bootstrap mutation succeeds
    httpMock.expectOne('/graphql').flush({
      data: { bootstrapPlatformAdmin: { token: 'x', expiresAt: '2099-01-01T00:00:00Z' } },
    });
    await flushMicrotasks();

    // 2) Auto-login mutation fails
    httpMock.expectOne('/graphql').flush(
      { errors: [{ message: 'Invalid credentials' }] },
      { status: 200, statusText: 'OK' },
    );
    await flushMicrotasks();

    expect(navSpy).toHaveBeenCalledWith([
      '/login',
    ], {
      queryParams: {
        notice: 'bootstrap-recovery',
        email: 'admin@example.com',
      },
      replaceUrl: true,
    });

    fixture.detectChanges();
    expect(comp.recoveryNotice()?.title).toBe('Automatic sign-in did not finish');
    expect((fixture.nativeElement as HTMLElement).querySelector('[data-testid="bootstrap-recovery-notice"]')?.textContent)
      .toContain('Bootstrap completed successfully. Sign in with the administrator credentials you just created to continue.');
  });

  it('suppresses duplicate bootstrap submits while completion is already in flight', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });

    comp.submit();
    comp.submit();

    const bootstrapRequest = httpMock.expectOne('/graphql');
    expect(bootstrapRequest.request.body.query).toContain('bootstrapPlatformAdmin');
    httpMock.expectNone('/graphql');
  });

  it('redirects to login with a bootstrap-unavailable notice when availability is already known to be false', () => {
    sessionBootstrapState.markBootstrapAvailability(false, 'GuardProbe');

    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    const fixture = TestBed.createComponent(BootstrapPage);
    fixture.detectChanges();

    expect(navSpy).toHaveBeenCalledWith([
      '/login',
    ], {
      queryParams: { notice: 'bootstrap-unavailable' },
      replaceUrl: true,
    });

    expect((fixture.nativeElement as HTMLElement).querySelector('[data-testid="bootstrap-unavailable-notice"]')?.textContent)
      .toContain('Bootstrap is no longer available. Sign in with an existing platform administrator account to continue.');
  });

  it('redirects to login with a bootstrap-unavailable notice when completion becomes unavailable during submit', async () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    httpMock.expectOne('/graphql').flush(
      {
        errors: [
          {
            message: 'Bootstrap is no longer available',
            extensions: {
              code: 'CONFLICT',
            },
          },
        ],
      },
      { status: 200, statusText: 'OK' },
    );
    await flushMicrotasks();

    expect(navSpy).toHaveBeenCalledWith([
      '/login',
    ], {
      queryParams: { notice: 'bootstrap-unavailable' },
      replaceUrl: true,
    });

    fixture.detectChanges();
    expect(comp.bootstrapUnavailableNotice()?.category).toBe('BootstrapUnavailable');
    expect((fixture.nativeElement as HTMLElement).querySelector('[data-testid="bootstrap-unavailable-notice"]')?.textContent)
      .toContain('Bootstrap already completed');
  });

  it('password toggle switches input type', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    fixture.detectChanges();

    const pwInput = (fixture.nativeElement as HTMLElement).querySelector<HTMLInputElement>('#bs-password');
    expect(pwInput?.type).toBe('password');

    comp.showPw.set(true);
    fixture.detectChanges();
    expect(pwInput?.type).toBe('text');
  });

  it('shows validation errors when form is submitted with empty fields', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    fixture.detectChanges();

    // Submit with empty form
    comp.submit();
    fixture.detectChanges();

    const errors = (fixture.nativeElement as HTMLElement).querySelectorAll('.text-error');
    expect(errors.length).toBeGreaterThanOrEqual(3);
  });

  it('submit button is disabled while pending', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    comp.pending.set(true);
    fixture.detectChanges();

    const btn = (fixture.nativeElement as HTMLElement).querySelector<HTMLButtonElement>('[type="submit"]');
    expect(btn?.disabled).toBe(true);
  });

  it('exposes required accessible labels and semantics for bootstrap controls', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const form = root.querySelector('form');
    const email = root.querySelector<HTMLInputElement>('#bs-email');
    const displayName = root.querySelector<HTMLInputElement>('#bs-displayName');
    const password = root.querySelector<HTMLInputElement>('#bs-password');
    const toggle = root.querySelector<HTMLButtonElement>('#bs-password-toggle');
    const submit = root.querySelector<HTMLButtonElement>('#bs-submit');

    expect(form?.getAttribute('aria-label')).toBe('Platform bootstrap form');
    expect(root.querySelector('label[for="bs-email"]')?.textContent).toContain('Email');
    expect(root.querySelector('label[for="bs-displayName"]')?.textContent).toContain('Display Name');
    expect(root.querySelector('label[for="bs-password"]')?.textContent).toContain('Password');
    expect(email?.getAttribute('aria-required')).toBe('true');
    expect(displayName?.getAttribute('aria-required')).toBe('true');
    expect(password?.getAttribute('aria-required')).toBe('true');
    expect(email?.getAttribute('aria-describedby')).toContain('bs-email-help');
    expect(displayName?.getAttribute('aria-describedby')).toContain('bs-displayName-help');
    expect(password?.getAttribute('aria-describedby')).toContain('bs-password-help');
    expect(toggle?.getAttribute('aria-label')).toBe('Show password');
    expect(submit?.getAttribute('aria-label')).toBe('Complete platform bootstrap');
    expect(submit?.className).toContain('focus-visible:outline');
  });

  it('keeps keyboard focus order in a linear top-down sequence', () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const focusables = Array.from(
      root.querySelectorAll<HTMLElement>('#bs-email, #bs-displayName, #bs-password, #bs-password-toggle, #bs-submit'),
    );

    expect(focusables.map((el) => el.id)).toEqual([
      'bs-email',
      'bs-displayName',
      'bs-password',
      'bs-password-toggle',
      'bs-submit',
    ]);
    expect(focusables.every((el) => el.tabIndex >= 0)).toBe(true);
  });

  it('maps GraphQL validation field errors onto typed form controls', async () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    httpMock.expectOne('/graphql').flush(
      {
        errors: [
          {
            message: 'Validation failed',
            extensions: {
              code: 'VALIDATION_FAILED',
              fieldErrors: {
                email: ['Email is already in use.'],
                password: ['Password must include a symbol.'],
              },
            },
          },
        ],
      },
      { status: 200, statusText: 'OK' },
    );
    await flushMicrotasks();

    expect(comp.email.errors?.['server']).toBe('Email is already in use.');
    expect(comp.password.errors?.['server']).toBe('Password must include a symbol.');
    expect(comp.globalError()).toBe('Validation failed');
  });

  it('surfaces non-validation GraphQL errors as global alerts without mutating field errors', async () => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    httpMock.expectOne('/graphql').flush(
      {
        errors: [
          {
            message: 'Bootstrap is no longer available.',
            extensions: {
              code: 'CONFLICT',
            },
          },
        ],
      },
      { status: 200, statusText: 'OK' },
    );
    await flushMicrotasks();

    expect(navSpy).toHaveBeenCalledWith([
      '/login',
    ], {
      queryParams: { notice: 'bootstrap-unavailable' },
      replaceUrl: true,
    });
    expect(comp.globalError()).toBeNull();
    expect(comp.bootstrapUnavailableNotice()?.title).toBe('Bootstrap already completed');
    expect(comp.email.errors?.['server']).toBeUndefined();
    expect(comp.password.errors?.['server']).toBeUndefined();
  });
});

