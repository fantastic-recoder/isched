import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient, withInterceptors } from '@angular/common/http';
import { HttpClient } from '@angular/common/http';
import { authInterceptor } from './auth.interceptor';
import { AuthService } from '../services/auth.service';

describe('authInterceptor', () => {
  let http: HttpClient;
  let httpMock: HttpTestingController;
  let authService: AuthService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        AuthService,
        provideHttpClient(withInterceptors([authInterceptor])),
        provideHttpClientTesting(),
      ],
    });

    http = TestBed.inject(HttpClient);
    httpMock = TestBed.inject(HttpTestingController);
    authService = TestBed.inject(AuthService);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('adds X-CSRF-Token for GraphQL mutations', () => {
    authService.setCsrfToken('csrf_token');

    http.post('/graphql', { query: 'mutation { logout }' }).subscribe();

    const req = httpMock.expectOne('/graphql');
    expect(req.request.headers.get('X-CSRF-Token')).toBe('csrf_token');
    expect(req.request.withCredentials).toBe(true);
    req.flush({ data: {} });
  });

  it('does not add X-CSRF-Token for GraphQL queries', () => {
    authService.setCsrfToken('csrf_token');

    http.post('/graphql', { query: '{ version }' }).subscribe();

    const req = httpMock.expectOne('/graphql');
    expect(req.request.headers.has('X-CSRF-Token')).toBe(false);
    expect(req.request.withCredentials).toBe(true);
    req.flush({ data: {} });
  });
});

