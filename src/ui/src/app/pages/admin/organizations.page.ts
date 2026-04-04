import { ChangeDetectionStrategy, Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { Organization, OrganizationService } from '../../services/organization.service';

@Component({
  selector: 'app-organizations-page',
  standalone: true,
  imports: [CommonModule],
  template: `
    <section class="p-6">
      <h1 class="text-2xl font-semibold">Organizations</h1>
      @if (error()) {
        <div class="alert alert-error mt-3">{{ error() }}</div>
      }
      <ul class="menu bg-base-100 rounded-box mt-4">
        @for (org of organizations(); track org.id) {
          <li>{{ org.name }} ({{ org.status }})</li>
        }
      </ul>
    </section>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class OrganizationsPage {
  private readonly organizationService = inject(OrganizationService);

  readonly organizations = signal<Organization[]>([]);
  readonly error = signal<string | null>(null);

  constructor() {
    this.organizationService.list().subscribe({
      next: ({ organizations }) => this.organizations.set(organizations),
      error: (err: Error) => this.error.set(err.message),
    });
  }
}

