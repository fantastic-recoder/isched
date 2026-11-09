// SPDX-License-Identifier: MPL-2.0
/**
 * @file query-editor.ts
 * @brief CodeMirror 6 wrapper component for the GraphQL query editor (SP-011).
 */

import {
  AfterViewInit,
  ChangeDetectionStrategy,
  Component,
  ElementRef,
  EventEmitter,
  Input,
  OnChanges,
  OnDestroy,
  Output,
  SimpleChanges,
  ViewChild,
} from '@angular/core';
import { EditorState } from '@codemirror/state';
import { basicSetup, EditorView } from 'codemirror';
import { getNamedType, parse, type GraphQLSchema, TypeInfo, visit, visitWithTypeInfo } from 'graphql';
import { graphql, updateSchema } from 'cm6-graphql';
import { hoverTooltip } from '@codemirror/view';

export type SchemaStatus = 'loading' | 'ready' | 'unavailable';

@Component({
  selector: 'app-query-editor',
  standalone: true,
  imports: [],
  templateUrl: './query-editor.html',
  styleUrl: './query-editor.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
  host: { class: 'flex flex-col h-full min-h-0' },
})
export class QueryEditorComponent implements AfterViewInit, OnChanges, OnDestroy {
  /** Set this to programmatically replace the editor content */
  @Input() initialContent = '';
  /** Active schema obtained from GraphQL introspection; null disables type-aware help. */
  @Input() schema: GraphQLSchema | null = null;
  /** Describes whether schema-backed editor assistance is active. */
  @Input() schemaStatus: SchemaStatus = 'unavailable';
  /** Safe user-facing explanation when schema loading or construction fails. */
  @Input() schemaError: string | null = null;
  /** Emitted (debounced) whenever the editor content changes */
  @Output() contentChange = new EventEmitter<string>();

  @ViewChild('editorHost') editorHostRef!: ElementRef<HTMLDivElement>;

  private view: EditorView | null = null;
  private debounceTimer: ReturnType<typeof setTimeout> | null = null;
  private pendingContent: string | null = null;

  ngAfterViewInit(): void {
    const startDoc = this.initialContent || '';
    const state = EditorState.create({
      doc: startDoc,
      extensions: [
        basicSetup,
        graphql(this.schema ?? undefined),
        hoverTooltip((view, position) => {
          if (!this.schema || this.schemaStatus !== 'ready') return null;
          const description = describeHoverAtPosition(
            this.schema,
            view.state.doc.toString(),
            position,
          );
          if (!description) return null;
          const dom = view.dom.ownerDocument.createElement('div');
          dom.className = 'cm-graphql-description';
          dom.textContent = description;
          return { pos: position, create: () => ({ dom }) };
        }),
        EditorView.updateListener.of((update) => {
          if (update.docChanged) {
            this.scheduleEmit(update.view.state.doc.toString());
          }
        }),
      ],
    });

    this.view = new EditorView({
      state,
      parent: this.editorHostRef.nativeElement,
    });

    if (this.pendingContent !== null) {
      this.setContent(this.pendingContent);
      this.pendingContent = null;
    }
  }

  ngOnChanges(changes: SimpleChanges): void {
    if ('schema' in changes && this.view) {
      updateSchema(this.view, this.schema ?? undefined);
    }
    if ('initialContent' in changes && this.view) {
      this.setContent(this.initialContent);
    } else if ('initialContent' in changes) {
      this.pendingContent = this.initialContent;
    }
  }

  ngOnDestroy(): void {
    if (this.debounceTimer !== null) {
      clearTimeout(this.debounceTimer);
    }
    this.view?.destroy();
    this.view = null;
  }

  /** Get the current editor content. */
  getContent(): string {
    return this.view?.state.doc.toString() ?? '';
  }

  private setContent(content: string): void {
    if (!this.view) return;
    this.view.dispatch({
      changes: {
        from: 0,
        to: this.view.state.doc.length,
        insert: content,
      },
    });
  }

  private scheduleEmit(content: string): void {
    if (this.debounceTimer !== null) {
      clearTimeout(this.debounceTimer);
    }
    this.debounceTimer = setTimeout(() => {
      this.debounceTimer = null;
      this.contentChange.emit(content);
    }, 300);
  }
}

/**
 * Return schema documentation for the GraphQL symbol under the document offset.
 * The helper deliberately returns null when the schema, document, or symbol
 * description is unavailable instead of inventing documentation.
 */
export function describeHoverAtPosition(
  schema: GraphQLSchema | null,
  source: string,
  position: number,
): string | null {
  if (!schema || position < 0 || position > source.length) return null;

  try {
    const typeInfo = new TypeInfo(schema);
    let description: string | null = null;
    visit(
      parse(source),
      visitWithTypeInfo(typeInfo, {
        Field(node) {
          if (isPositionWithin(node.name.loc, position)) {
            description = typeInfo.getFieldDef()?.description ?? null;
          }
        },
        Argument(node) {
          if (isPositionWithin(node.name.loc, position)) {
            description = typeInfo.getArgument()?.description ?? null;
          }
        },
        EnumValue(node) {
          if (isPositionWithin(node.loc, position)) {
            const inputType = typeInfo.getInputType();
            if (inputType) {
              const namedType = getNamedType(inputType);
              if ('getValues' in namedType && typeof namedType.getValues === 'function') {
                description =
                  namedType.getValues().find((value) => value.name === node.value)?.description ?? null;
              }
            }
          }
        },
        NamedType(node) {
          if (isPositionWithin(node.loc, position)) {
            description = schema.getType(node.name.value)?.description ?? null;
          }
        },
      }),
    );
    return description;
  } catch {
    // CodeMirror's language parser provides diagnostics for incomplete documents.
    return null;
  }
}

function isPositionWithin(
  location: { start: number; end: number } | undefined,
  position: number,
): boolean {
  return location !== undefined && location.start <= position && position <= location.end;
}
