import {
  mapAuthAttemptOutcome,
  mapAuthErrorToAlert,
  mapBootstrapErrorToAlert,
} from './auth-alert.mapper';
import { GRAPHQL_ERROR_CODES, GraphQLRequestError } from './graphql.service';

describe('auth-alert.mapper', () => {
  it('maps RATE_LIMITED auth errors to deterministic alert copy with retry metadata', () => {
    const error = new GraphQLRequestError(
      'Too many authentication attempts',
      GRAPHQL_ERROR_CODES.RATE_LIMITED,
      {},
      42000,
    );

    const alert = mapAuthErrorToAlert(error);

    expect(alert.category).toBe('AuthRateLimited');
    expect(alert.kind).toBe('Warning');
    expect(alert.body).toContain('about 42 seconds');
    expect(alert.retryAfterMs).toBe(42000);
  });

  it('maps RATE_LIMITED auth outcomes with fallback copy when retry metadata is absent', () => {
    const error = new GraphQLRequestError('Rate limited', GRAPHQL_ERROR_CODES.RATE_LIMITED);

    const outcome = mapAuthAttemptOutcome(error);

    expect(outcome.status).toBe('RateLimited');
    expect(outcome.errorCode).toBe(GRAPHQL_ERROR_CODES.RATE_LIMITED);
    expect(outcome.guidanceText).toBe(
      'Too many failed sign-in attempts. Please wait a few minutes before trying again.',
    );
  });

  it('maps CONFLICT bootstrap errors to bootstrap-unavailable alert', () => {
    const error = new GraphQLRequestError('Bootstrap is no longer available', GRAPHQL_ERROR_CODES.CONFLICT);

    const alert = mapBootstrapErrorToAlert(error);

    expect(alert.category).toBe('BootstrapUnavailable');
    expect(alert.title).toBe('Bootstrap already completed');
  });
});

