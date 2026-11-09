// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground.ts
 * @brief GraphQL Playground page — authenticated route component (SP-011).
 */

import {
  ChangeDetectionStrategy,
  Component,
  DestroyRef,
  OnInit,
  ViewChild,
  inject,
  computed,
  signal,
} from '@angular/core';
import { Router } from '@angular/router';
import { CommonModule } from '@angular/common';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';

import { AuthService } from '../../services/auth.service';
import { SessionBootstrapStateService } from '../../services/session-bootstrap-state.service';
import { PlaygroundIntrospectionService } from '../../services/playground-introspection.service';
import { PlaygroundQueryService, PlaygroundQueryResult } from '../../services/playground-query.service';
import { PlaygroundLayoutService } from '../../services/playground-layout.service';
import { SchemaTreeNode } from '../../services/playground-introspection.models';
import { ResizableSplitComponent } from '../../components/playground/resizable-split/resizable-split';
import { SchemaTreeComponent } from '../../components/playground/schema-tree/schema-tree';
import { QueryEditorComponent } from '../../components/playground/query-editor/query-editor';
import { ResultPanelComponent } from '../../components/playground/result-panel/result-panel';
import { generateStub } from './query-stub-generator';

@Component({
  selector: 'app-playground',
  standalone: true,
  imports: [
    CommonModule,
    ResizableSplitComponent,
    SchemaTreeComponent,
    QueryEditorComponent,
    ResultPanelComponent,
  ],
  templateUrl: './playground.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class PlaygroundPage implements OnInit {
  private readonly introspection = inject(PlaygroundIntrospectionService);
  private readonly queryService = inject(PlaygroundQueryService);
  readonly layout = inject(PlaygroundLayoutService);
  private readonly destroyRef = inject(DestroyRef);
  private readonly auth = inject(AuthService);
  private readonly sessionState = inject(SessionBootstrapStateService);
  private readonly router = inject(Router);

  @ViewChild('editor') editorRef!: QueryEditorComponent;

  readonly treeNodes = this.introspection.treeNodes;
  readonly treeLoading = this.introspection.loading;
  readonly treeError = this.introspection.error;
  readonly activeSchema = this.introspection.schema;
  readonly schemaStatus = this.introspection.schemaStatus;

  readonly queryResult = signal<PlaygroundQueryResult | null>(null);
  readonly isRunning = signal(false);
  readonly editorContent = signal('');
  readonly canGenerate = signal(false);
  /** Operation lifecycle: idle -> submitting -> success | error | reauth-required. */
  readonly status = signal<PlaygroundStatus>('idle');
  /** Recoverable failure (transport/rate-limit): the preserved editor content can be re-submitted. */
  readonly canRetry = computed(() => this.status() === 'error' && this.queryResult()?.retryable === true);
  /** Last submitted content, preserved so retry re-submits exactly what failed. */
  private lastSubmitted = '';
  private selectedTreeNode: SchemaTreeNode | null = null;

  ngOnInit(): void {
    const draft = this.sessionState.takePlaygroundDraft();
    if (draft) {
      this.editorContent.set(draft);
    }
    this.introspection.load();
    this.introspection.watchPluginRegistry().pipe(takeUntilDestroyed(this.destroyRef)).subscribe();
  }

  onEditorContentChange(content: string): void {
    this.editorContent.set(content);
  }

  onNodeSelected(node: SchemaTreeNode | null): void {
    this.selectedTreeNode = node;
    this.canGenerate.set(node !== null);
  }

  onGenerateQuery(node: SchemaTreeNode): void {
    const stub = generateStub(node);
    if (this.editorRef) {
      this.editorRef.initialContent = stub;
    }
    this.editorContent.set(stub);
  }

  generateQueryFromToolbar(): void {
    if (this.selectedTreeNode) {
      this.onGenerateQuery(this.selectedTreeNode);
    }
  }

  onLeftResize(pct: number): void {
    this.layout.setLeftWidth(pct);
  }

  onTopResize(pct: number): void {
    this.layout.setTopHeight(pct);
  }

  runQuery(): void {
    if (this.isRunning()) return;

    const content = this.editorRef?.getContent() ?? this.editorContent();
    if (!content.trim()) return;

    this.lastSubmitted = content;
    this.isRunning.set(true);
    this.status.set('submitting');
    this.queryResult.set({ loading: true });

    this.queryService
      .execute(content)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.queryResult.set(result);
          if (!result.loading) {
            this.isRunning.set(false);
            this.applyOutcome(result, content);
          }
        },
        error: () => {
          this.queryResult.set({
            loading: false,
            errors: ['Unexpected error running query'],
            errorCategory: 'unknown',
            retryable: false,
          });
          this.status.set('error');
          this.isRunning.set(false);
        },
      });
  }

  /** Re-submits the content that failed (editor content is never cleared on failure). */
  retry(): void {
    if (!this.canRetry() || this.isRunning()) return;
    if (this.lastSubmitted) {
      this.editorContent.set(this.lastSubmitted);
    }
    this.runQuery();
  }

  private applyOutcome(result: PlaygroundQueryResult, content: string): void {
    if (result.reauthRequired) {
      // Session expired: clear in-memory credentials, keep the draft, go sign in again.
      this.status.set('reauth-required');
      this.sessionState.savePlaygroundDraft(content);
      this.auth.handleReauthRequired(this.router.url);
      void this.router.navigate(['/login'], {
        queryParams: { reason: 'reauth-required', returnUrl: this.router.url },
      });
      return;
    }
    const failed = (result.errors?.length ?? 0) > 0;
    this.status.set(failed ? 'error' : 'success');
  }
}

export type PlaygroundStatus = 'idle' | 'submitting' | 'success' | 'error' | 'reauth-required';
