import { ChangeDetectionStrategy, Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { Organization, OrganizationService } from '../../services/organization.service';

@Component({
  selector: 'app-organizations-page',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './organizations.page.html',
  styleUrl: './organizations.page.scss',
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

