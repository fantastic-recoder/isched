import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter, Router } from '@angular/router';
import { BootstrapPage } from './bootstrap.page';

/** Flush microtask queue so chained observables propagate. */
const flushMicrotasks = () => new Promise<void>((r) => setTimeout(r, 0));

describe('BootstrapPage', () => {
  let httpMock: HttpTestingController;
  let router: Router;

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

    expect(navSpy).toHaveBeenCalledWith(['/login']);
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
});

