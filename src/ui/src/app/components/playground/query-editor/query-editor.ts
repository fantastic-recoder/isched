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
import { graphql } from 'cm6-graphql';

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
        graphql(),
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

