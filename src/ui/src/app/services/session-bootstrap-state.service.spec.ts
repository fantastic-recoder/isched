import { TestBed } from '@angular/core/testing';
import { SessionBootstrapStateService } from './session-bootstrap-state.service';

describe('SessionBootstrapStateService', () => {
  let service: SessionBootstrapStateService;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [SessionBootstrapStateService] });
    service = TestBed.inject(SessionBootstrapStateService);
  });

  it('tracks startup bootstrap availability and session authentication state', () => {
    service.markBootstrapAvailability(true, 'StartupProbe');
    service.markSessionKnown(true);
    service.markInitialRouteResolved();

    const state = service.sessionBootstrapState();
    const eligibility = service.bootstrapEligibilityState();

    expect(state.seedModeActive).toBe(true);
    expect(state.sessionKnown).toBe(true);
    expect(state.sessionAuthenticated).toBe(true);
    expect(state.initialRouteResolved).toBe(true);
    expect(eligibility.isAvailable).toBe(true);
    expect(eligibility.source).toBe('StartupProbe');
  });

  it('tracks first guarded revalidation and supports sign-out-style clearing', () => {
    service.markSessionKnown(true);
    service.markFirstGuardRevalidationComplete();

    expect(service.sessionBootstrapState().firstGuardRevalidationComplete).toBe(true);

    service.clearSessionIndicators();

    const state = service.sessionBootstrapState();
    expect(state.sessionKnown).toBe(false);
    expect(state.sessionAuthenticated).toBe(false);
    expect(state.firstGuardRevalidationComplete).toBe(false);
  });

  it('resets all state to initial defaults', () => {
    service.markBootstrapAvailability(true, 'GuardProbe');
    service.markSessionKnown(true);

    service.reset();

    const state = service.sessionBootstrapState();
    const eligibility = service.bootstrapEligibilityState();

    expect(state.seedModeActive).toBe(false);
    expect(state.sessionKnown).toBe(false);
    expect(state.initialRouteResolved).toBe(false);
    expect(eligibility.isAvailable).toBe(false);
    expect(eligibility.source).toBe('StartupProbe');
  });
});

