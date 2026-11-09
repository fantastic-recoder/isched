import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { signal } from '@angular/core';
import { DevProxyHealthPage } from './dev-proxy-health.page';
import {
  GraphQLSubscriptionService,
  SubscriptionConnection,
} from '../../services/graphql-subscription.service';

describe('DevProxyHealthPage', () => {
  const connection = signal<SubscriptionConnection>('closed');

  beforeEach(async () => {
    connection.set('closed');
    await TestBed.configureTestingModule({
      imports: [DevProxyHealthPage],
      providers: [
        provideHttpClient(),
        { provide: GraphQLSubscriptionService, useValue: { connection, subscribe: jest.fn() } },
      ],
    }).compileComponents();
  });

  it('renders proxy diagnostics guidance', () => {
    const fixture = TestBed.createComponent(DevProxyHealthPage);
    fixture.detectChanges();

    const text = (fixture.nativeElement as HTMLElement).textContent ?? '';
    expect(text).toContain('/graphql');
    expect(text).toContain('WebSocket');
  });

  it('shows no failure banner unless the subscription connection failed', () => {
    const fixture = TestBed.createComponent(DevProxyHealthPage);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="ws-failed-banner"]')).toBeNull();
    expect(host.querySelector('[data-testid="ws-state"]')?.textContent).toContain('closed');
  });

  it('shows a non-blocking diagnostic banner when the WebSocket failed (HTTP flows unaffected)', () => {
    connection.set('failed');
    const fixture = TestBed.createComponent(DevProxyHealthPage);
    fixture.detectChanges();
    const banner = (fixture.nativeElement as HTMLElement).querySelector('[data-testid="ws-failed-banner"]');
    expect(banner).not.toBeNull();
    expect(banner?.textContent).toContain('keep working');
    expect((fixture.nativeElement as HTMLElement).querySelector('[data-testid="ws-probe-btn"]')).not.toBeNull();
  });
});
