import { Component } from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-login',
  standalone: true,
  imports: [CommonModule],
  template: `<div class="min-h-screen flex items-center justify-center bg-base-200">
    <div class="card w-full max-w-md bg-base-100 shadow-xl">
      <div class="card-body">
        <h2 class="card-title">Sign In</h2>
        <p class="text-sm text-base-content/60">Sign in to isched administration.</p>
      </div>
    </div>
  </div>`,
})
export class LoginComponent {}
