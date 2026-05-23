// SPDX-License-Identifier: MPL-2.0
/**
 * @file result-panel.spec.ts
 * @brief Unit tests for ResultPanelComponent (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ResultPanelComponent } from './result-panel';
import { PlaygroundQueryResult } from '../../../services/playground-query.service';

describe('ResultPanelComponent', () => {
  let fixture: ComponentFixture<ResultPanelComponent>;
  let component: ResultPanelComponent;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [ResultPanelComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(ResultPanelComponent);
    component = fixture.componentInstance;
  });

  it('shows idle placeholder when result is null', () => {
    component.result = null;
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="result-idle"]')).toBeTruthy();
  });

  it('shows loading spinner when result.loading is true', () => {
    component.result = { loading: true };
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="result-loading"]')).toBeTruthy();
    expect(host.querySelector('[data-testid="result-idle"]')).toBeNull();
  });

  it('shows subscription advisory when isSubscriptionAdvisory is true', () => {
    component.result = { loading: false, isSubscriptionAdvisory: true };
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="result-subscription-advisory"]')).toBeTruthy();
  });

  it('shows errors when errors array is populated', () => {
    component.result = { loading: false, errors: ['Field not found', 'Auth error'] };
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    const errEl = host.querySelector('[data-testid="result-errors"]');
    expect(errEl).toBeTruthy();
    expect(errEl?.textContent).toContain('Field not found');
    expect(errEl?.textContent).toContain('Auth error');
  });

  it('shows formatted JSON when data is present', () => {
    const result: PlaygroundQueryResult = {
      loading: false,
      data: { health: { status: 'UP' } },
    };
    component.result = result;
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    const jsonEl = host.querySelector('[data-testid="result-json"]');
    expect(jsonEl).toBeTruthy();
    expect(jsonEl?.textContent).toContain('"health"');
    expect(jsonEl?.textContent).toContain('"UP"');
  });

  it('shows expand button for large payloads', () => {
    const bigData: Record<string, string> = {};
    for (let i = 0; i < 3000; i++) {
      bigData[`key${i}`] = 'value'.repeat(10);
    }
    component.result = { loading: false, data: bigData };
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(component.isLargePayload).toBe(true);
    const expandBtn = host.querySelector('[data-testid="result-expand-btn"]');
    expect(expandBtn).toBeTruthy();
  });

  it('expand button reveals full payload', () => {
    const bigData: Record<string, string> = {};
    for (let i = 0; i < 3000; i++) {
      bigData[`key${i}`] = 'value'.repeat(10);
    }
    component.result = { loading: false, data: bigData };
    fixture.detectChanges();

    expect(component._expanded()).toBe(false);
    component.expandAll();
    expect(component._expanded()).toBe(true);
  });

  it('resets expanded state when result is replaced', () => {
    const bigData: Record<string, string> = {};
    for (let i = 0; i < 3000; i++) {
      bigData[`key${i}`] = 'value'.repeat(10);
    }
    component.result = { loading: false, data: bigData };
    fixture.detectChanges();
    component.expandAll();

    component.result = { loading: false, data: { small: true } };
    expect(component._expanded()).toBe(false);
  });
});

