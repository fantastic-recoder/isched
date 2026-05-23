// SPDX-License-Identifier: MPL-2.0
/**
 * @file resizable-split.ts
 * @brief Reusable two-panel resizable split component for the playground (SP-011).
 */

import {
  AfterViewInit,
  ChangeDetectionStrategy,
  Component,
  ElementRef,
  EventEmitter,
  Input,
  OnDestroy,
  Output,
  ViewChild,
  signal,
} from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-resizable-split',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './resizable-split.html',
  styleUrl: './resizable-split.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ResizableSplitComponent implements AfterViewInit, OnDestroy {
  /** 'vertical' = left/right split; 'horizontal' = top/bottom split */
  @Input() direction: 'vertical' | 'horizontal' = 'vertical';
  /** Initial size of the first panel as a percentage */
  @Input() set initialSizePct(value: number) {
    this._firstSizePct.set(this.clamp(value));
  }
  /** Emits the first-panel percentage when the user finishes dragging */
  @Output() sizeChange = new EventEmitter<number>();

  @ViewChild('container') containerRef!: ElementRef<HTMLElement>;

  readonly _firstSizePct = signal(50);
  readonly _dragging = signal(false);

  private boundMouseMove!: (e: MouseEvent) => void;
  private boundMouseUp!: (e: MouseEvent) => void;

  ngAfterViewInit(): void {
    this.boundMouseMove = (e) => this.onMouseMove(e);
    this.boundMouseUp = () => this.onMouseUp();
  }

  ngOnDestroy(): void {
    this.cleanupListeners();
  }

  onDividerMouseDown(event: MouseEvent): void {
    event.preventDefault();
    this._dragging.set(true);
    document.addEventListener('mousemove', this.boundMouseMove);
    document.addEventListener('mouseup', this.boundMouseUp);
  }

  private onMouseMove(event: MouseEvent): void {
    if (!this._dragging()) return;
    const container = this.containerRef?.nativeElement;
    if (!container) return;

    const rect = container.getBoundingClientRect();
    let pct: number;

    if (this.direction === 'vertical') {
      pct = ((event.clientX - rect.left) / rect.width) * 100;
    } else {
      pct = ((event.clientY - rect.top) / rect.height) * 100;
    }

    this._firstSizePct.set(this.clamp(pct));
  }

  private onMouseUp(): void {
    if (!this._dragging()) return;
    this._dragging.set(false);
    this.cleanupListeners();
    this.sizeChange.emit(this._firstSizePct());
  }

  private cleanupListeners(): void {
    document.removeEventListener('mousemove', this.boundMouseMove);
    document.removeEventListener('mouseup', this.boundMouseUp);
  }

  private clamp(value: number): number {
    const min = this.direction === 'vertical' ? 20 : 15;
    const max = this.direction === 'vertical' ? 80 : 85;
    return Math.min(Math.max(value, min), max);
  }

  get firstStyle(): string {
    const pct = this._firstSizePct();
    return this.direction === 'vertical'
      ? `width: ${pct}%`
      : `height: ${pct}%`;
  }

  get secondStyle(): string {
    const pct = 100 - this._firstSizePct();
    return this.direction === 'vertical'
      ? `width: ${pct}%`
      : `height: ${pct}%`;
  }

  get dividerCursor(): string {
    return this.direction === 'vertical' ? 'col-resize' : 'row-resize';
  }
}

