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
  signal,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';

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
  styleUrl: './playground.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class PlaygroundPage implements OnInit {
  private readonly introspection = inject(PlaygroundIntrospectionService);
  private readonly queryService = inject(PlaygroundQueryService);
  readonly layout = inject(PlaygroundLayoutService);
  private readonly destroyRef = inject(DestroyRef);

  @ViewChild('editor') editorRef!: QueryEditorComponent;

  readonly treeNodes = this.introspection.treeNodes;
  readonly treeLoading = this.introspection.loading;
  readonly treeError = this.introspection.error;

  readonly queryResult = signal<PlaygroundQueryResult | null>(null);
  readonly isRunning = signal(false);
  readonly editorContent = signal('');

  ngOnInit(): void {
    this.introspection.load();
  }

  onEditorContentChange(content: string): void {
    this.editorContent.set(content);
  }

  onNodeSelected(_node: SchemaTreeNode | null): void {
    // Reserved for future extension
  }

  onGenerateQuery(node: SchemaTreeNode): void {
    const stub = generateStub(node);
    if (this.editorRef) {
      this.editorRef.initialContent = stub;
    }
    this.editorContent.set(stub);
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

    this.isRunning.set(true);
    this.queryResult.set({ loading: true });

    this.queryService
      .execute(content)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.queryResult.set(result);
          if (!result.loading) {
            this.isRunning.set(false);
          }
        },
        error: () => {
          this.queryResult.set({ loading: false, errors: ['Unexpected error running query'] });
          this.isRunning.set(false);
        },
      });
  }
}

