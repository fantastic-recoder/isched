// SPDX-License-Identifier: MPL-2.0
/**
 * @file resizable-split.spec.ts
 * @brief Unit tests for ResizableSplitComponent (SP-011)
 */

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ResizableSplitComponent } from './resizable-split';

describe('ResizableSplitComponent', () => {
  let fixture: ComponentFixture<ResizableSplitComponent>;
  let component: ResizableSplitComponent;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [ResizableSplitComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(ResizableSplitComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('defaults to 50% first panel size', () => {
    expect(component._firstSizePct()).toBe(50);
  });

  it('accepts initialSizePct input', () => {
    component.initialSizePct = 40;
    expect(component._firstSizePct()).toBe(40);
  });

  it('clamps initialSizePct to minimum for vertical (20)', () => {
    component.direction = 'vertical';
    component.initialSizePct = 5;
    expect(component._firstSizePct()).toBe(20);
  });

  it('clamps initialSizePct to maximum for vertical (80)', () => {
    component.direction = 'vertical';
    component.initialSizePct = 95;
    expect(component._firstSizePct()).toBe(80);
  });

  it('clamps initialSizePct to minimum for horizontal (15)', () => {
    component.direction = 'horizontal';
    component.initialSizePct = 5;
    expect(component._firstSizePct()).toBe(15);
  });

  it('clamps initialSizePct to maximum for horizontal (85)', () => {
    component.direction = 'horizontal';
    component.initialSizePct = 95;
    expect(component._firstSizePct()).toBe(85);
  });

  it('produces col-resize cursor for vertical direction', () => {
    component.direction = 'vertical';
    expect(component.dividerCursor).toBe('col-resize');
  });

  it('produces row-resize cursor for horizontal direction', () => {
    component.direction = 'horizontal';
    expect(component.dividerCursor).toBe('row-resize');
  });

  it('sets dragging state on mousedown', () => {
    const event = new MouseEvent('mousedown', { clientX: 0, clientY: 0 });
    jest.spyOn(event, 'preventDefault');
    component.onDividerMouseDown(event);
    expect(component._dragging()).toBe(true);
    // cleanup
    document.dispatchEvent(new MouseEvent('mouseup'));
  });

  it('firstStyle contains width for vertical split', () => {
    component.direction = 'vertical';
    component.initialSizePct = 30;
    expect(component.firstStyle).toContain('width: 30%');
  });

  it('firstStyle contains height for horizontal split', () => {
    component.direction = 'horizontal';
    component.initialSizePct = 60;
    expect(component.firstStyle).toContain('height: 60%');
  });

  it('secondStyle is complement of first', () => {
    component.direction = 'vertical';
    component.initialSizePct = 30;
    expect(component.secondStyle).toContain('width: 70%');
  });

  it('emits sizeChange on mouseup after drag', () => {
    const sizes: number[] = [];
    component.sizeChange.subscribe((n) => sizes.push(n));

    component.initialSizePct = 40;
    const down = new MouseEvent('mousedown', { clientX: 0, clientY: 0 });
    jest.spyOn(down, 'preventDefault');
    component.onDividerMouseDown(down);

    document.dispatchEvent(new MouseEvent('mouseup'));

    expect(sizes.length).toBe(1);
    expect(component._dragging()).toBe(false);
  });
});

