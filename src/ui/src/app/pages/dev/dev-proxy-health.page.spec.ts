import { TestBed } from '@angular/core/testing';
import { DevProxyHealthPage } from './dev-proxy-health.page';

describe('DevProxyHealthPage', () => {
  it('renders proxy diagnostics guidance', async () => {
    await TestBed.configureTestingModule({ imports: [DevProxyHealthPage] }).compileComponents();
    const fixture = TestBed.createComponent(DevProxyHealthPage);
    fixture.detectChanges();

    const text = (fixture.nativeElement as HTMLElement).textContent ?? '';
    expect(text).toContain('/graphql');
    expect(text).toContain('WebSocket');
  });
});

