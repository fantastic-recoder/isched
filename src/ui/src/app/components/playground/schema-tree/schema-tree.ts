// SPDX-License-Identifier: MPL-2.0
/**
 * @file schema-tree.ts
 * @brief Renders the collapsible GraphQL schema tree with Generate Query button (SP-011).
 */

import {
  ChangeDetectionStrategy,
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  signal,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { SchemaTreeNode } from '../../../services/playground-introspection.models';

@Component({
  selector: 'app-schema-tree',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './schema-tree.html',
  styleUrl: './schema-tree.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class SchemaTreeComponent implements OnChanges {
  @Input() nodes: SchemaTreeNode[] = [];
  @Input() loading = false;
  @Input() error: string | null = null;

  @Output() nodeSelected = new EventEmitter<SchemaTreeNode | null>();
  @Output() generateQuery = new EventEmitter<SchemaTreeNode>();

  readonly selectedNodeId = signal<string | null>(null);
  readonly collapsedIds = signal<Set<string>>(new Set());

  ngOnChanges(): void {
    // Reset selection when tree data changes
    this.selectedNodeId.set(null);
  }

  selectNode(node: SchemaTreeNode): void {
    if (!node.isSelectable) return;
    this.selectedNodeId.set(node.id);
    this.nodeSelected.emit(node);
  }

  onNodeActivate(node: SchemaTreeNode, event?: Event): void {
    event?.preventDefault();
    event?.stopPropagation();
    this.selectNode(node);
  }

  toggleCollapse(node: SchemaTreeNode): void {
    this.collapsedIds.update((prev) => {
      const next = new Set(prev);
      if (next.has(node.id)) {
        next.delete(node.id);
      } else {
        next.add(node.id);
      }
      return next;
    });
  }

  isCollapsed(node: SchemaTreeNode): boolean {
    return this.collapsedIds().has(node.id);
  }

  isSelected(node: SchemaTreeNode): boolean {
    return this.selectedNodeId() === node.id;
  }

  get selectedNode(): SchemaTreeNode | null {
    const id = this.selectedNodeId();
    if (!id) return null;
    return this.findNode(id, this.nodes);
  }

  onGenerateQuery(): void {
    const node = this.selectedNode;
    if (node) {
      this.generateQuery.emit(node);
    }
  }

  private findNode(id: string, nodes: SchemaTreeNode[]): SchemaTreeNode | null {
    for (const node of nodes) {
      if (node.id === id) return node;
      const found = this.findNode(id, node.children);
      if (found) return found;
    }
    return null;
  }

  trackById = (_: number, node: SchemaTreeNode): string => node.id;
}

