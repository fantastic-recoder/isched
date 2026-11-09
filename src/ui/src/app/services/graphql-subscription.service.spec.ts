import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { GraphQLSubscriptionService, WS_CLOSE_UNAUTHORIZED } from './graphql-subscription.service';

class FakeWebSocket {
  static instances: FakeWebSocket[] = [];
  static readonly CONNECTING = 0;
  static readonly OPEN = 1;
  readyState = 0;
  sent: string[] = [];
  onopen: (() => void) | null = null;
  onmessage: ((e: { data: string }) => void) | null = null;
  onclose: ((e: { code: number }) => void) | null = null;
  onerror: (() => void) | null = null;
  constructor(public url: string, public protocols: string[]) {
    FakeWebSocket.instances.push(this);
  }
  send(data: string) { this.sent.push(data); }
  close() { this.readyState = 3; }
  open() { this.readyState = 1; this.onopen?.(); }
}

describe('GraphQLSubscriptionService', () => {
  let service: GraphQLSubscriptionService;
  const realWs = (globalThis as { WebSocket?: unknown }).WebSocket;

  beforeEach(() => {
    FakeWebSocket.instances = [];
    (globalThis as { WebSocket?: unknown }).WebSocket = FakeWebSocket;
    (window as unknown as { WebSocket: unknown }).WebSocket = FakeWebSocket;
    TestBed.configureTestingModule({ providers: [provideHttpClient()] });
    service = TestBed.inject(GraphQLSubscriptionService);
  });

  afterEach(() => {
    service.ngOnDestroy();
    (globalThis as { WebSocket?: unknown }).WebSocket = realWs;
    (window as unknown as { WebSocket: unknown }).WebSocket = realWs;
  });

  it('authenticates through the browser cookie and sends no credential in connection_init', () => {
    const sub = service.subscribe('subscription { x }').subscribe({ error: () => undefined });
    const ws = FakeWebSocket.instances[0];
    expect(ws.url).toMatch(/^wss?:\/\/[^?]+\/graphql$/);
    ws.open();
    expect(JSON.parse(ws.sent[0])).toEqual({ type: 'connection_init' });
    expect(ws.sent[0]).not.toMatch(/token|authorization|bearer/i);
    sub.unsubscribe();
  });

  it('tracks connecting -> open and goes failed on 4401 without reconnecting', () => {
    const errors: unknown[] = [];
    const sub = service.subscribe('subscription { x }').subscribe({ error: (e) => errors.push(e) });
    expect(service.connection()).toBe('connecting');
    const ws = FakeWebSocket.instances[0];
    ws.open();
    ws.onclose?.({ code: WS_CLOSE_UNAUTHORIZED });
    expect(service.connection()).toBe('failed');
    expect(errors.length).toBe(1);
    jest.useFakeTimers();
    jest.advanceTimersByTime(60_000);
    jest.useRealTimers();
    expect(FakeWebSocket.instances.length).toBe(1);
    sub.unsubscribe();
  });

  it('reports failed when the socket closes before ever opening (proxy upgrade failure)', () => {
    const sub = service.subscribe('subscription { x }').subscribe({ error: () => undefined });
    FakeWebSocket.instances[0].onclose?.({ code: 1006 });
    expect(service.connection()).toBe('failed');
    sub.unsubscribe();
  });
});
