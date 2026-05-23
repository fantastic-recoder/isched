// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-query.service.spec.ts
 * @brief Unit tests for PlaygroundQueryService (SP-011)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { PlaygroundQueryService } from './playground-query.service';
import { firstValueFrom, toArray } from 'rxjs';

describe('PlaygroundQueryService', () => {
  let service: PlaygroundQueryService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        PlaygroundQueryService,
        provideHttpClient(),
        provideHttpClientTesting(),
      ],
    });
    service = TestBed.inject(PlaygroundQueryService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('returns subscription advisory immediately without HTTP request for subscription queries', async () => {
    const result = await firstValueFrom(service.execute('subscription { events { id } }'));
    expect(result.isSubscriptionAdvisory).toBe(true);
    expect(result.loading).toBe(false);
    httpMock.expectNone('/graphql');
  });

  it('detects subscription even with leading whitespace', async () => {
    const result = await firstValueFrom(service.execute('   subscription OnEvent { id }'));
    expect(result.isSubscriptionAdvisory).toBe(true);
    httpMock.expectNone('/graphql');
  });

  it('emits loading:true then data on success', async () => {
    const results$ = service.execute('{ health { status } }').pipe(toArray());

    const promise = firstValueFrom(results$);
    const req = httpMock.expectOne('/graphql');
    req.flush({ data: { health: { status: 'UP' } } });

    const results = await promise;
    expect(results[0]).toEqual({ loading: true });
    expect(results[1]).toEqual({ loading: false, data: { health: { status: 'UP' } } });
  });

  it('maps GraphQL errors array to errors field', async () => {
    const results$ = service.execute('{ broken }').pipe(toArray());
    const promise = firstValueFrom(results$);

    httpMock.expectOne('/graphql').flush({ errors: [{ message: 'Field not found' }] });

    const results = await promise;
    const last = results[results.length - 1];
    expect(last.errors).toContain('Field not found');
    expect(last.loading).toBe(false);
  });

  it('converts HTTP network error to errors field', async () => {
    const results$ = service.execute('{ health { status } }').pipe(toArray());
    const promise = firstValueFrom(results$);

    httpMock.expectOne('/graphql').error(new ProgressEvent('network error'));

    const results = await promise;
    const last = results[results.length - 1];
    expect(last.errors).toBeDefined();
    expect(last.loading).toBe(false);
  });

  it('handles query operations without treating them as subscriptions', async () => {
    const results$ = service.execute('query { health { status } }').pipe(toArray());
    const promise = firstValueFrom(results$);

    const req = httpMock.expectOne('/graphql');
    expect(req.request.body.query).toBe('query { health { status } }');
    req.flush({ data: { health: { status: 'UP' } } });

    await promise;
  });
});

