import { Component } from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-seed',
  standalone: true,
  imports: [CommonModule],
  template: `<div class="min-h-screen flex items-center justify-center bg-base-200">
    <div class="card w-full max-w-md bg-base-100 shadow-xl">
      <div class="card-body">
        <h2 class="card-title">Create Platform Administrator</h2>
        <p class="text-sm text-base-content/60">First-run setup — no administrator exists yet.</p>
      </div>
    </div>
  </div>`,
})
export class SeedComponent {}
