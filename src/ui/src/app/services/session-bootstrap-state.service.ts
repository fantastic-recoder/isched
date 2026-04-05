import { Injectable, computed, signal } from '@angular/core';
import {
  BootstrapEligibilitySource,
  BootstrapEligibilityState,
  INITIAL_BOOTSTRAP_ELIGIBILITY_STATE,
  INITIAL_SESSION_BOOTSTRAP_STATE,
  SessionBootstrapState,
} from './auth-bootstrap.models';

@Injectable({ providedIn: 'root' })
export class SessionBootstrapStateService {
  private readonly sessionBootstrapStateSignal = signal<SessionBootstrapState>(
    INITIAL_SESSION_BOOTSTRAP_STATE,
  );

  private readonly bootstrapEligibilitySignal = signal<BootstrapEligibilityState>(
    INITIAL_BOOTSTRAP_ELIGIBILITY_STATE,
  );

  readonly sessionBootstrapState = this.sessionBootstrapStateSignal.asReadonly();
  readonly bootstrapEligibilityState = this.bootstrapEligibilitySignal.asReadonly();

  readonly bootstrapFirstRoutingRequired = computed(() =>
    this.sessionBootstrapStateSignal().seedModeActive,
  );

  markBootstrapAvailability(isAvailable: boolean, source: BootstrapEligibilitySource): void {
    this.bootstrapEligibilitySignal.set({
      isAvailable,
      source,
      checkedAt: new Date().toISOString(),
    });

    this.sessionBootstrapStateSignal.update((state) => ({
      ...state,
      seedModeActive: isAvailable,
    }));
  }

  markSessionKnown(sessionAuthenticated: boolean): void {
    this.sessionBootstrapStateSignal.update((state) => ({
      ...state,
      sessionKnown: true,
      sessionAuthenticated,
    }));
  }

  markInitialRouteResolved(): void {
    this.sessionBootstrapStateSignal.update((state) => ({
      ...state,
      initialRouteResolved: true,
    }));
  }

  markFirstGuardRevalidationComplete(): void {
    this.sessionBootstrapStateSignal.update((state) => ({
      ...state,
      firstGuardRevalidationComplete: true,
    }));
  }

  clearSessionIndicators(): void {
    this.sessionBootstrapStateSignal.update((state) => ({
      ...state,
      sessionKnown: false,
      sessionAuthenticated: false,
      firstGuardRevalidationComplete: false,
    }));
  }

  reset(): void {
    this.sessionBootstrapStateSignal.set(INITIAL_SESSION_BOOTSTRAP_STATE);
    this.bootstrapEligibilitySignal.set(INITIAL_BOOTSTRAP_ELIGIBILITY_STATE);
  }
}

