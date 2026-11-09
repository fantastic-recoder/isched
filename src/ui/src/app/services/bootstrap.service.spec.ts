import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { BootstrapService } from './bootstrap.service';
import { GraphQLRequestError, GraphQLService } from './graphql.service';
import { SessionBootstrapStateService } from './session-bootstrap-state.service';

describe('BootstrapService', () => {
  let service: BootstrapService;
  let httpMock: HttpTestingController;
  const sessionBootstrapStateMock = {
    markBootstrapAvailability: jest.fn(),
  };

  beforeEach(() => {
    sessionBootstrapStateMock.markBootstrapAvailability.mockReset();
    TestBed.configureTestingModule({
      providers: [
        BootstrapService,
        GraphQLService,
        provideHttpClient(),
        provideHttpClientTesting(),
        { provide: SessionBootstrapStateService, useValue: sessionBootstrapStateMock },
      ],
    });

    service = TestBed.inject(BootstrapService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('queries seed mode via namespaced isched.systemState', () => {
    service.bootstrapStatus('StartupProbe').subscribe((result) => {
      expect(result.systemState.seedModeActive).toBe(true);
    });

    const req = httpMock.expectOne('/graphql');
    expect(req.request.method).toBe('POST');
    expect(req.request.body.query).toContain('isched { systemState { seedModeActive } }');

    req.flush({
      data: {
        isched: {
          systemState: { seedModeActive: true },
        },
      },
    });

    expect(sessionBootstrapStateMock.markBootstrapAvailability).toHaveBeenCalledWith(true, 'StartupProbe');
  });

  it('completes bootstrap via namespaced isched.bootstrapPlatformAdmin', () => {
    service.completeBootstrap({
      email: 'admin@example.com',
      password: 'SecurePass1!',
      displayName: 'Admin',
    }).subscribe((result) => {
      expect(result.bootstrapPlatformAdmin.csrfToken).toBe('session-csrf');
    });

    const req = httpMock.expectOne('/graphql');
    expect(req.request.body.query).toContain('isched {');
    expect(req.request.body.query).toContain('bootstrapPlatformAdmin');
    expect(req.request.body.query).toContain('csrfToken');
    expect(req.request.body.query).not.toContain('expiresAt');

    req.flush({
      data: {
        isched: {
          bootstrapPlatformAdmin: {
            csrfToken: 'session-csrf',
          },
        },
      },
    });

    expect(sessionBootstrapStateMock.markBootstrapAvailability).toHaveBeenCalledWith(false, 'ActionProbe');
  });

  it('does not fallback to legacy non-namespaced queries', (done) => {
    service.bootstrapStatus('StartupProbe').subscribe({
      next: () => done.fail('expected error'),
      error: (err: GraphQLRequestError) => {
        expect(err).toBeInstanceOf(GraphQLRequestError);
        expect(err.message).toContain('Cannot query field "isched"');
        done();
      },
    });

    const req = httpMock.expectOne('/graphql');
    req.flush({
      errors: [
        { message: 'Cannot query field "isched" on type "Query".' },
      ],
    });

    httpMock.expectNone('/graphql');
  });
});
