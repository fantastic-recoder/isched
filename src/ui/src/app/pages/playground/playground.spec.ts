// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground.spec.ts
 * @brief Smoke tests for PlaygroundPage (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideHttpClientTesting } from '@angular/common/http/testing';
import { PlaygroundPage } from './playground';
import { PlaygroundIntrospectionService } from '../../services/playground-introspection.service';

describe('PlaygroundPage', () => {
  let fixture: ComponentFixture<PlaygroundPage>;
  let component: PlaygroundPage;
  let introspectionSpy: jest.SpyInstance;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [PlaygroundPage],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
      ],
    }).compileComponents();

    const introspectionService = TestBed.inject(PlaygroundIntrospectionService);
    introspectionSpy = jest.spyOn(introspectionService, 'load').mockImplementation(() => {});

    fixture = TestBed.createComponent(PlaygroundPage);
    component = fixture.componentInstance;
  });

  it('calls introspection.load() on init', () => {
    fixture.detectChanges();
    expect(introspectionSpy).toHaveBeenCalledTimes(1);
  });

  it('renders the playground page element', () => {
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="playground-page"]')).toBeTruthy();
  });

  it('renders the Run query button', () => {
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[data-testid="run-query-btn"]')).toBeTruthy();
  });

  it('Run button is disabled while isRunning', () => {
    fixture.detectChanges();
    component.isRunning.set(true);
    fixture.detectChanges();

    const btn = fixture.nativeElement.querySelector('[data-testid="run-query-btn"]') as HTMLButtonElement;
    expect(btn.disabled).toBe(true);
  });

  it('stores generate query result in editorContent signal', () => {
    fixture.detectChanges();
    component.onGenerateQuery({
      id: 'field:query:health',
      kind: 'field',
      name: 'health',
      typeName: 'Health',
      args: [],
      children: [],
      isSelectable: true,
      operationKind: 'query',
    });
    expect(component.editorContent()).toContain('health');
  });

  it('updates layout on left resize event', () => {
    fixture.detectChanges();
    const layout = component.layout;
    component.onLeftResize(45);
    expect(layout.layout().leftWidthPct).toBe(45);
  });

  it('updates layout on top resize event', () => {
    fixture.detectChanges();
    component.onTopResize(60);
    expect(component.layout.layout().topHeightPct).toBe(60);
  });
});

