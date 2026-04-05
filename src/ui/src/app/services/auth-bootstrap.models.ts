export type AuthAttemptStatus = 'Success' | 'InvalidCredentials' | 'RateLimited' | 'TransportFailure';

export interface AuthAttemptOutcome {
  status: AuthAttemptStatus;
  errorCode?: string;
  message: string;
  retryAfterMs?: number;
  guidanceText: string;
  occurredAt: string;
}

export type AlertKind = 'Info' | 'Warning' | 'Error' | 'Success';

export type UserFacingAlertCategory =
  | 'AuthRateLimited'
  | 'AuthFailure'
  | 'BootstrapUnavailable'
  | 'SessionExpired'
  | 'Generic';

export interface UserFacingAlert {
  kind: AlertKind;
  category: UserFacingAlertCategory;
  title: string;
  body: string;
  retryAfterMs?: number;
  dismissible: boolean;
}

export interface SessionBootstrapState {
  seedModeActive: boolean;
  sessionKnown: boolean;
  sessionAuthenticated: boolean;
  initialRouteResolved: boolean;
  firstGuardRevalidationComplete: boolean;
}

export type BootstrapEligibilitySource = 'StartupProbe' | 'GuardProbe' | 'ActionProbe';

export interface BootstrapEligibilityState {
  isAvailable: boolean;
  checkedAt: string;
  source: BootstrapEligibilitySource;
}

export type AuthFlowName = 'SignIn' | 'BootstrapComplete';

export interface AuthFlowFlightState {
  flow: AuthFlowName;
  inFlight: boolean;
  requestId?: string;
  startedAt?: string;
  lastResolvedAt?: string;
}

export const INITIAL_SESSION_BOOTSTRAP_STATE: SessionBootstrapState = {
  seedModeActive: false,
  sessionKnown: false,
  sessionAuthenticated: false,
  initialRouteResolved: false,
  firstGuardRevalidationComplete: false,
};

export const INITIAL_BOOTSTRAP_ELIGIBILITY_STATE: BootstrapEligibilityState = {
  isAvailable: false,
  checkedAt: new Date(0).toISOString(),
  source: 'StartupProbe',
};

