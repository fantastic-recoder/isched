// SPDX-License-Identifier: MPL-2.0
/**
 * @file result-panel.ts
 * @brief Displays query execution results, loading, errors, or subscription advisory (SP-011).
 */

import { ChangeDetectionStrategy, Component, Input, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { PlaygroundQueryResult } from '../../../services/playground-query.service';

const LARGE_PAYLOAD_THRESHOLD = 100_000; // characters

@Component({
  selector: 'app-result-panel',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './result-panel.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ResultPanelComponent {
  @Input() set result(value: PlaygroundQueryResult | null) {
    this._result = value;
    this._expanded.set(false);
    this._jsonString = this.formatJson(value?.data);
  }

  get result(): PlaygroundQueryResult | null { return this._result; }

  private _result: PlaygroundQueryResult | null = null;
  readonly _expanded = signal(false);

  _jsonString = '';

  get isLargePayload(): boolean {
    return this._jsonString.length > LARGE_PAYLOAD_THRESHOLD;
  }

  get displayJson(): string {
    if (this._expanded() || !this.isLargePayload) return this._jsonString;
    return this._jsonString.slice(0, LARGE_PAYLOAD_THRESHOLD) + '\n\n… (truncated — click Expand All to view)';
  }

  expandAll(): void {
    this._expanded.set(true);
  }

  private formatJson(data: unknown): string {
    if (data === null || data === undefined) return '';
    try {
      return JSON.stringify(data, null, 2);
    } catch {
      return String(data);
    }
  }
}

