import { GRAPHQL_ERROR_CODES, GraphQLRequestError } from './graphql.service';
import { AuthAttemptOutcome, UserFacingAlert } from './auth-bootstrap.models';

const LOCKOUT_FALLBACK_COPY =
  'Too many failed sign-in attempts. Please wait a few minutes before trying again.';

function buildRateLimitedGuidance(retryAfterMs?: number): string {
  if (typeof retryAfterMs !== 'number' || retryAfterMs <= 0) {
    return LOCKOUT_FALLBACK_COPY;
  }

  const retryAfterSeconds = Math.max(1, Math.ceil(retryAfterMs / 1000));
  return `Too many failed sign-in attempts. Try again in about ${retryAfterSeconds} second${retryAfterSeconds === 1 ? '' : 's'}.`;
}

export function mapAuthAttemptOutcome(error: GraphQLRequestError): AuthAttemptOutcome {
  const isRateLimited = error.code === GRAPHQL_ERROR_CODES.RATE_LIMITED;

  return {
    status: isRateLimited ? 'RateLimited' : 'InvalidCredentials',
    errorCode: error.code,
    message: error.message,
    retryAfterMs: error.retryAfterMs,
    guidanceText: isRateLimited ? buildRateLimitedGuidance(error.retryAfterMs) : 'Sign-in failed. Check your credentials and try again.',
    occurredAt: new Date().toISOString(),
  };
}

export function mapAuthErrorToAlert(error: unknown): UserFacingAlert {
  if (error instanceof GraphQLRequestError && error.code === GRAPHQL_ERROR_CODES.RATE_LIMITED) {
    return {
      kind: 'Warning',
      category: 'AuthRateLimited',
      title: 'Sign-in temporarily locked',
      body: buildRateLimitedGuidance(error.retryAfterMs),
      retryAfterMs: error.retryAfterMs,
      dismissible: true,
    };
  }

  if (error instanceof GraphQLRequestError && error.code === GRAPHQL_ERROR_CODES.UNAUTHENTICATED) {
    return {
      kind: 'Error',
      category: 'SessionExpired',
      title: 'Session expired',
      body: 'Your session is no longer valid. Please sign in again.',
      dismissible: true,
    };
  }

  if (error instanceof GraphQLRequestError) {
    return {
      kind: 'Error',
      category: 'AuthFailure',
      title: 'Sign-in failed',
      body: error.message || 'Unable to sign in right now. Please try again.',
      dismissible: true,
    };
  }

  return {
    kind: 'Error',
    category: 'Generic',
    title: 'Unexpected error',
    body: error instanceof Error && error.message ? error.message : 'Something went wrong. Please try again.',
    dismissible: true,
  };
}

export function mapBootstrapErrorToAlert(error: unknown): UserFacingAlert {
  if (error instanceof GraphQLRequestError && error.code === GRAPHQL_ERROR_CODES.CONFLICT) {
    return {
      kind: 'Info',
      category: 'BootstrapUnavailable',
      title: 'Bootstrap already completed',
      body: 'Bootstrap is no longer available. Sign in with an existing account to continue.',
      dismissible: true,
    };
  }

  if (error instanceof GraphQLRequestError) {
    return {
      kind: 'Error',
      category: 'Generic',
      title: 'Bootstrap failed',
      body: error.message || 'Bootstrap could not be completed. Please try again.',
      dismissible: true,
    };
  }

  return {
    kind: 'Error',
    category: 'Generic',
    title: 'Bootstrap failed',
    body: error instanceof Error && error.message ? error.message : 'Bootstrap could not be completed. Please try again.',
    dismissible: true,
  };
}

