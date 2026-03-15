import { Component } from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [CommonModule],
  template: `<div class="min-h-screen bg-base-200">
    <div class="navbar bg-base-100 shadow">
      <div class="flex-1"><span class="text-xl font-bold">isched</span></div>
      <div class="flex-none">
        <button class="btn btn-ghost btn-sm">Sign out</button>
      </div>
    </div>
    <main class="container mx-auto p-6">
      <h1 class="text-2xl font-bold mb-4">Dashboard</h1>
    </main>
  </div>`,
})
export class DashboardComponent {}
