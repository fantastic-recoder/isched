// SPDX-License-Identifier: MPL-2.0
/**
 * @file schema-tree.spec.ts
 * @brief Unit tests for SchemaTreeComponent (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { SchemaTreeComponent } from './schema-tree';
import { SchemaTreeNode } from '../../../services/playground-introspection.models';

function queryField(name: string): SchemaTreeNode {
  return {
    id: `field:query:${name}`,
    kind: 'field',
    name,
    typeName: 'String',
    args: [],
    children: [],
    isSelectable: true,
    operationKind: 'query',
  };
}

function groupNode(name: string, children: SchemaTreeNode[]): SchemaTreeNode {
  return {
    id: `group:${name}`,
    kind: 'operationGroup',
    name,
    children,
    isSelectable: false,
  };
}

describe('SchemaTreeComponent', () => {
  let fixture: ComponentFixture<SchemaTreeComponent>;
  let component: SchemaTreeComponent;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SchemaTreeComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(SchemaTreeComponent);
    component = fixture.componentInstance;
  });

  it('shows loading skeleton when loading=true', () => {
    component.loading = true;
    component.nodes = [];
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[aria-label="Loading schema"]')).toBeTruthy();
  });

  it('shows error alert when error is set', () => {
    component.loading = false;
    component.error = 'Introspection failed';
    component.nodes = [];
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.querySelector('[role="alert"]')?.textContent).toContain('Introspection failed');
  });

  it('renders operation group labels', () => {
    component.nodes = [
      groupNode('Queries', [queryField('health'), queryField('version')]),
    ];
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.textContent).toContain('Queries');
    expect(host.textContent).toContain('health');
    expect(host.textContent).toContain('version');
  });

  it('Generate Query button is disabled when nothing is selected', () => {
    component.nodes = [groupNode('Queries', [queryField('health')])];
    fixture.detectChanges();

    const btn = fixture.nativeElement.querySelector('[data-testid="generate-query-btn"]') as HTMLButtonElement;
    expect(btn.disabled).toBe(true);
  });

  it('Generate Query button is enabled after selecting a selectable node', () => {
    component.nodes = [groupNode('Queries', [queryField('health')])];
    fixture.detectChanges();

    const field = queryField('health');
    component.selectNode(field);
    component.selectedNodeId.set(field.id);
    fixture.detectChanges();

    // Force override nodes so findNode can find it
    component.nodes = [groupNode('Queries', [field])];
    fixture.detectChanges();

    const btn = fixture.nativeElement.querySelector('[data-testid="generate-query-btn"]') as HTMLButtonElement;
    expect(btn.disabled).toBe(false);
  });

  it('emits nodeSelected when a selectable node is clicked', () => {
    const selected: SchemaTreeNode[] = [];
    component.nodeSelected.subscribe((n) => { if (n) selected.push(n); });

    const field = queryField('health');
    component.selectNode(field);

    expect(selected.length).toBe(1);
    expect(selected[0].name).toBe('health');
  });

  it('does NOT emit nodeSelected when a non-selectable group node is clicked', () => {
    const selected: SchemaTreeNode[] = [];
    component.nodeSelected.subscribe((n) => { if (n) selected.push(n); });

    const group = groupNode('Queries', []);
    component.selectNode(group);

    expect(selected.length).toBe(0);
  });

  it('emits generateQuery when generate button is clicked with a selected node', () => {
    const generated: SchemaTreeNode[] = [];
    component.generateQuery.subscribe((n) => generated.push(n));

    const field = queryField('health');
    component.nodes = [groupNode('Queries', [field])];
    component.selectedNodeId.set(field.id);
    fixture.detectChanges();

    component.onGenerateQuery();

    expect(generated.length).toBe(1);
    expect(generated[0].name).toBe('health');
  });

  it('toggles node collapse state', () => {
    const group = groupNode('Queries', [queryField('health')]);
    expect(component.isCollapsed(group)).toBe(false);

    component.toggleCollapse(group);
    expect(component.isCollapsed(group)).toBe(true);

    component.toggleCollapse(group);
    expect(component.isCollapsed(group)).toBe(false);
  });
});

