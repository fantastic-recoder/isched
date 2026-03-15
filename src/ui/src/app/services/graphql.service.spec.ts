// SPDX-License-Identifier: MPL-2.0
/**
 * @file graphql.service.spec.ts
 * @brief Unit tests for GraphQLService (T-UI-F-010)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { GraphQLService } from './graphql.service';

describe('GraphQLService', () => {
  let service: GraphQLService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [GraphQLService, provideHttpClient(), provideHttpClientTesting()],
    });
    service = TestBed.inject(GraphQLService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('should POST to /graphql with query and variables', () => {
    const doc = '{ hello }';
    const vars = { foo: 'bar' };

    service.query<{ hello: string }>(doc, vars).subscribe();

    const req = httpMock.expectOne('/graphql');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ query: doc, variables: vars });
    req.flush({ data: { hello: 'world' } });
  });

  it('should unwrap data from a successful response', (done) => {
    service.query<{ hello: string }>('{ hello }').subscribe((data) => {
      expect(data).toEqual({ hello: 'world' });
      done();
    });

    httpMock.expectOne('/graphql').flush({ data: { hello: 'world' } });
  });

  it('should surface the first GraphQL error as a thrown Error', (done) => {
    service.query<unknown>('{ broken }').subscribe({
      error: (err: Error) => {
        expect(err.message).toBe('Something went wrong');
        done();
      },
    });

    httpMock.expectOne('/graphql').flush({
      errors: [{ message: 'Something went wrong' }],
    });
  });

  it('mutate() also posts to /graphql', () => {
    service.mutate<unknown>('mutation { logout }').subscribe();

    const req = httpMock.expectOne('/graphql');
    expect(req.request.method).toBe('POST');
    req.flush({ data: {} });
  });
});
