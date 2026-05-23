// SPDX-License-Identifier: MPL-2.0
/**
 * @file query-editor.spec.ts
 * @brief Unit tests for QueryEditorComponent (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { QueryEditorComponent } from './query-editor';

describe('QueryEditorComponent', () => {
  let fixture: ComponentFixture<QueryEditorComponent>;
  let component: QueryEditorComponent;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [QueryEditorComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(QueryEditorComponent);
    component = fixture.componentInstance;
  });

  it('creates without error (CodeMirror mocked)', () => {
    fixture.detectChanges();
    expect(component).toBeTruthy();
  });

  it('getContent returns empty string before initialisation', () => {
    expect(component.getContent()).toBe('');
  });

  it('emits contentChange after content is set (debounced)', async () => {
    jest.useFakeTimers();
    fixture.detectChanges();

    const emitted: string[] = [];
    component.contentChange.subscribe((v) => emitted.push(v));

    // Simulate the debounce emit path directly
    (component as unknown as { scheduleEmit: (s: string) => void })['scheduleEmit']('query { health }');
    jest.advanceTimersByTime(400); // past 300ms debounce

    expect(emitted).toContain('query { health }');
    jest.useRealTimers();
  });

  it('setting initialContent on existing view calls setContent', () => {
    fixture.detectChanges(); // triggers ngAfterViewInit

    const dispatchSpy = jest.fn();
    const destroySpy = jest.fn();
    // Inject a mock view with dispatch and destroy
    (component as unknown as { view: { dispatch: jest.Mock; destroy: jest.Mock; state: { doc: { length: number } } } })['view'] = {
      dispatch: dispatchSpy,
      destroy: destroySpy,
      state: { doc: { length: 5 } },
    };

    component.initialContent = 'mutation { login }';
    component.ngOnChanges({
      initialContent: {
        currentValue: 'mutation { login }',
        previousValue: '',
        firstChange: false,
        isFirstChange: () => false,
      },
    });

    expect(dispatchSpy).toHaveBeenCalledWith(
      expect.objectContaining({
        changes: expect.objectContaining({ insert: 'mutation { login }' }),
      }),
    );
  });
});

