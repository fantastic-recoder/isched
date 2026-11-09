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

  /** True after a protected operation reported UNAUTHENTICATED/401 (spec 013). In-memory only. */
  private readonly reauthRequiredSignal = signal(false);
  private readonly returnUrlSignal = signal<string | null>(null);
  private readonly playgroundDraftSignal = signal<string | null>(null);

  readonly reauthRequired = this.reauthRequiredSignal.asReadonly();
  readonly returnUrl = this.returnUrlSignal.asReadonly();

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

  markReauthRequired(returnUrl?: string | null): void {
    this.reauthRequiredSignal.set(true);
    if (returnUrl && isSafeReturnUrl(returnUrl)) {
      this.returnUrlSignal.set(returnUrl);
    }
  }

  clearReauthRequired(): void {
    this.reauthRequiredSignal.set(false);
    this.returnUrlSignal.set(null);
  }

  /** Keeps unsaved Playground editor content (memory only) across a re-auth redirect. */
  savePlaygroundDraft(content: string): void {
    this.playgroundDraftSignal.set(content.length > 0 ? content : null);
  }

  /** Returns and clears the saved Playground draft. */
  takePlaygroundDraft(): string | null {
    const draft = this.playgroundDraftSignal();
    this.playgroundDraftSignal.set(null);
    return draft;
  }

  reset(): void {
    this.reauthRequiredSignal.set(false);
    this.returnUrlSignal.set(null);
    this.playgroundDraftSignal.set(null);
    this.sessionBootstrapStateSignal.set(INITIAL_SESSION_BOOTSTRAP_STATE);
    this.bootstrapEligibilitySignal.set(INITIAL_BOOTSTRAP_ELIGIBILITY_STATE);
  }
}


/** Only same-origin absolute paths may be used as post-login return URLs. */
export function isSafeReturnUrl(url: string): boolean {
  return url.startsWith('/') && !url.startsWith('//') && !url.includes('\\') && !url.startsWith('/login');
}
