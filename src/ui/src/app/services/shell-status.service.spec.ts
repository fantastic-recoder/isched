import { TestBed } from '@angular/core/testing';
import { ShellStatusService } from './shell-status.service';

describe('ShellStatusService', () => {
  let service: ShellStatusService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(ShellStatusService);
    service.reset();
  });

  it('starts with a non-empty fallback identity and idle digest', () => {
    expect(service.identity().resolved).toBe(false);
    expect(service.identity().fallbackLabel).toBe('Signed-in user');
    expect(service.operationDigest()).toMatchObject({
      state: 'idle',
      message: 'Ready',
      operationKey: 'shell:idle',
      sequence: 0,
    });
  });

  it('keeps the newest operation digest when older operations complete later', () => {
    const firstSequence = service.beginOperation('organization-users:list', 'Loading organization users');
    const newerSequence = service.beginOperation('organization-users:list', 'Loading organization users');

    service.completeOperation('organization-users:list', 'Organization users loaded', firstSequence);
    expect(service.operationDigest()).toMatchObject({
      state: 'loading',
      message: 'Loading organization users',
      sequence: newerSequence,
    });

    service.completeOperation('organization-users:list', 'Organization users loaded', newerSequence);
    expect(service.operationDigest()).toMatchObject({
      state: 'success',
      message: 'Organization users loaded',
      sequence: newerSequence,
    });
  });

  it('records understandable failure digests for tracked operations', () => {
    const sequence = service.beginOperation('organization-users:list', 'Loading organization users');

    service.failOperation('organization-users:list', 'Unable to load organization users', sequence);

    expect(service.operationDigest()).toMatchObject({
      state: 'error',
      message: 'Unable to load organization users',
      operationKey: 'organization-users:list',
      sequence,
    });
  });

  it('replaces the identity fallback with the resolved display name and restores fallback on reset', () => {
    service.setIdentity({ userId: 'user-1', displayName: 'Jane Admin' });
    expect(service.identity()).toMatchObject({
      displayName: 'Jane Admin',
      userId: 'user-1',
      resolved: true,
    });

    service.reset();
    expect(service.identity()).toMatchObject({
      displayName: 'Signed-in user',
      fallbackLabel: 'Signed-in user',
      resolved: false,
    });
  });
});

