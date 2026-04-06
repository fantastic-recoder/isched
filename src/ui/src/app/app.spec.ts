import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { BehaviorSubject, tap } from 'rxjs';
import { App } from './app';
import { AuthService } from './services/auth.service';
import { BootstrapService } from './services/bootstrap.service';
import { SessionBootstrapStateService } from './services/session-bootstrap-state.service';
import { ShellStatusService } from './services/shell-status.service';

describe('App', () => {
  const status$ = new BehaviorSubject({ systemState: { seedModeActive: false } });
  const hint$ = new BehaviorSubject<boolean | null>(null);

  const bootstrapService = {
    bootstrapStatus: jest.fn((source: 'StartupProbe' | 'GuardProbe' | 'ActionProbe' = 'StartupProbe') =>
      status$.asObservable().pipe(
        tap(({ systemState }) => {
          hint$.next(systemState.seedModeActive);
          TestBed.inject(SessionBootstrapStateService).markBootstrapAvailability(
            systemState.seedModeActive,
            source,
          );
        }),
      ),
    ),
    seedModeHint$: jest.fn(() => hint$.asObservable()),
  };

  const authService = {
    isLoggedIn: jest.fn(() => false),
  };

  const flushStartup = async () => {
    await Promise.resolve();
    await Promise.resolve();
  };

  async function createApp(startUrl: string) {
    const router = TestBed.inject(Router);
    const navigateSpy = jest.spyOn(router, 'navigateByUrl').mockResolvedValue(true);
    const urlSpy = jest.spyOn(router, 'url', 'get').mockReturnValue(startUrl);

    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();
    await flushStartup();

    return {
      fixture,
      navigateSpy,
      urlSpy,
      sessionBootstrapState: TestBed.inject(SessionBootstrapStateService),
    };
  }

  beforeEach(async () => {
    jest.clearAllMocks();
    status$.next({ systemState: { seedModeActive: false } });
    hint$.next(null);
    authService.isLoggedIn.mockReturnValue(false);

    await TestBed.configureTestingModule({
      imports: [App],
      providers: [
        provideRouter([]),
        SessionBootstrapStateService,
        ShellStatusService,
        {
          provide: BootstrapService,
          useValue: bootstrapService,
        },
        {
          provide: AuthService,
          useValue: authService,
        },
      ],
    }).compileComponents();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('creates the app shell', async () => {
    const { fixture } = await createApp('/login');
    expect(fixture.componentInstance).toBeTruthy();
  });

  it('shows the shared authenticated shell on authenticated routes and hides it on public routes', async () => {
    authService.isLoggedIn.mockReturnValue(true);

    const dashboardApp = await createApp('/dashboard');
    expect(
      (dashboardApp.fixture.nativeElement as HTMLElement).querySelector('[data-testid="authenticated-shell"]'),
    ).not.toBeNull();

    authService.isLoggedIn.mockReturnValue(false);
    const loginApp = await createApp('/login');
    expect(
      (loginApp.fixture.nativeElement as HTMLElement).querySelector('[data-testid="authenticated-shell"]'),
    ).toBeNull();
  });

  it.each([
    {
      name: 'routes to bootstrap when seed mode is active and no session is available',
      startUrl: '/',
      seedModeActive: true,
      isLoggedIn: false,
      expectedNavigation: '/bootstrap',
      expectedState: { seedModeActive: true, sessionKnown: false, sessionAuthenticated: false },
    },
    {
      name: 'routes to login when seed mode is inactive and no session is available',
      startUrl: '/',
      seedModeActive: false,
      isLoggedIn: false,
      expectedNavigation: '/login',
      expectedState: { seedModeActive: false, sessionKnown: true, sessionAuthenticated: false },
    },
    {
      name: 'still routes to bootstrap when seed mode is active and a session exists',
      startUrl: '/dashboard',
      seedModeActive: true,
      isLoggedIn: true,
      expectedNavigation: '/bootstrap',
      expectedState: { seedModeActive: true, sessionKnown: false, sessionAuthenticated: false },
    },
    {
      name: 'keeps the protected destination when seed mode is inactive and a session exists',
      startUrl: '/admin/users',
      seedModeActive: false,
      isLoggedIn: true,
      expectedNavigation: null,
      expectedState: { seedModeActive: false, sessionKnown: true, sessionAuthenticated: true },
    },
  ])('$name', async ({ startUrl, seedModeActive, isLoggedIn, expectedNavigation, expectedState }) => {
    status$.next({ systemState: { seedModeActive } });
    authService.isLoggedIn.mockReturnValue(isLoggedIn);

    const { navigateSpy, sessionBootstrapState } = await createApp(startUrl);

    expect(bootstrapService.bootstrapStatus).toHaveBeenCalledWith('StartupProbe');
    if (expectedNavigation) {
      expect(navigateSpy).toHaveBeenCalledWith(expectedNavigation, { replaceUrl: true });
    } else {
      expect(navigateSpy).not.toHaveBeenCalled();
    }

    expect(sessionBootstrapState.sessionBootstrapState()).toMatchObject({
      ...expectedState,
      initialRouteResolved: true,
    });
  });

  it('switches off the bootstrap banner after bootstrap completes', async () => {
    status$.next({ systemState: { seedModeActive: true } });
    hint$.next(true);

    const { fixture } = await createApp('/login');
    const compiled = fixture.nativeElement as HTMLElement;

    expect(compiled.textContent).toContain('Bootstrap mode active');

    hint$.next(false);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(compiled.textContent).not.toContain('Bootstrap mode active');
  });
});
