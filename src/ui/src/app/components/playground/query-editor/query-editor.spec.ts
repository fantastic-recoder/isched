// SPDX-License-Identifier: MPL-2.0
/**
 * @file query-editor.spec.ts
 * @brief Unit tests for QueryEditorComponent (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { buildASTSchema, parse, validate } from 'graphql';
import { graphql } from 'cm6-graphql';
import { QueryEditorComponent } from './query-editor';
import { describeHoverAtPosition } from './query-editor';

describe('QueryEditorComponent', () => {
  let fixture: ComponentFixture<QueryEditorComponent>;
  let component: QueryEditorComponent;

  beforeEach(async () => {
    jest.clearAllMocks();
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

  it('uses the active schema for editor language support', () => {
    const schema = buildASTSchema(parse(`
      type Query {
        """Current service health."""
        health: String
      }
    `));
    component.schema = schema;
    component.schemaStatus = 'ready';

    fixture.detectChanges();

    expect(component.schemaStatus).toBe('ready');
    expect(component.schema).not.toBeNull();
    expect(graphql).toHaveBeenCalledWith(schema);
  });

  it('schema validation identifies syntax and type errors for editor diagnostics', () => {
    const schema = buildASTSchema(parse('type Query { health: String }'));
    component.schema = schema;
    component.schemaStatus = 'ready';
    fixture.detectChanges();

    const syntaxErrors = (() => {
      try {
        parse('query { health');
        return [];
      } catch (error) {
        return [error];
      }
    })();
    const typeErrors = validate(schema, parse('query { missingField }'));

    expect(syntaxErrors).toHaveLength(1);
    expect(typeErrors.map((error) => error.message).join(' ')).toContain('Cannot query field');
    expect(graphql).toHaveBeenCalledWith(schema);
  });

  it('shows an explicit unavailable state when no active schema exists', () => {
    fixture.detectChanges();

    const status = fixture.nativeElement.querySelector('[data-testid="schema-assistance-unavailable"]');
    expect(status).not.toBeNull();
    expect(status.textContent).toContain('completion and validation are unavailable');
    expect(graphql).toHaveBeenCalledWith(undefined);
  });

  it('returns field documentation at the hovered GraphQL field', () => {
    const schema = buildASTSchema(parse(`
      type Query {
        """Current service health."""
        health: String
      }
    `));
    const source = 'query { health }';

    expect(describeHoverAtPosition(schema, source, source.indexOf('health') + 2))
      .toBe('Current service health.');
  });

  it('does not invent documentation for undocumented fields', () => {
    const schema = buildASTSchema(parse('type Query { health: String }'));
    const source = 'query { health }';

    expect(describeHoverAtPosition(schema, source, source.indexOf('health') + 2)).toBeNull();
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
