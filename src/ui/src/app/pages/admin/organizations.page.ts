import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component, ElementRef, inject, signal, viewChild } from '@angular/core';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { Organization, OrganizationService } from '../../services/organization.service';

@Component({
  selector: 'app-organizations-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './organizations.page.html',
  styleUrl: './organizations.page.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class OrganizationsPage {
  private readonly organizationService = inject(OrganizationService);
  private readonly fb = inject(FormBuilder);
  private nextLocalOrgId = 1;

  readonly statusAlert = viewChild<ElementRef<HTMLElement>>('statusAlert');

  readonly organizations = signal<Organization[]>([]);
  readonly error = signal<string | null>(null);
  readonly statusMessage = signal<string | null>(null);
  readonly createForm = this.fb.nonNullable.group({
    name: ['', [Validators.required, Validators.minLength(2)]],
  });

  constructor() {
    this.organizationService.list().subscribe({
      next: ({ organizations }) => this.organizations.set(organizations),
      error: (err: Error) => this.error.set(err.message),
    });
  }

  createOrganization(): void {
    if (this.createForm.invalid) {
      this.createForm.markAllAsTouched();
      return;
    }

    const trimmedName = this.createForm.controls.name.value.trim();
    if (!trimmedName) {
      this.createForm.controls.name.setErrors({ required: true });
      this.createForm.controls.name.markAsTouched();
      return;
    }

    const organization: Organization = {
      id: `local-org-${this.nextLocalOrgId++}`,
      name: trimmedName,
      status: 'ACTIVE',
    };

    this.organizations.update((current) => [...current, organization]);
    this.createForm.reset({ name: '' });
    this.statusMessage.set(`Created organization ${organization.name}.`);
    this.focusStatusAlert();
  }

  editOrganization(orgId: string): void {
    const organization = this.organizations().find((item) => item.id === orgId);
    if (!organization) {
      return;
    }

    this.createForm.controls.name.setValue(organization.name);
    this.statusMessage.set(`Editing organization ${organization.name}.`);
    this.focusStatusAlert();
  }

  toggleOrganizationStatus(orgId: string): void {
    let updatedOrganizationName = '';
    let updatedStatus = '';

    this.organizations.update((current) =>
      current.map((organization) => {
        if (organization.id !== orgId) {
          return organization;
        }

        updatedOrganizationName = organization.name;
        updatedStatus = organization.status === 'ACTIVE' ? 'INACTIVE' : 'ACTIVE';

        return {
          ...organization,
          status: updatedStatus,
        };
      }),
    );

    if (!updatedOrganizationName) {
      return;
    }

    this.statusMessage.set(`${updatedOrganizationName} is now ${updatedStatus.toLowerCase()}.`);
    this.focusStatusAlert();
  }

  onEditKeydown(event: KeyboardEvent, orgId: string): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.editOrganization(orgId);
  }

  onToggleKeydown(event: KeyboardEvent, orgId: string): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.toggleOrganizationStatus(orgId);
  }

  private isActivationKey(event: KeyboardEvent): boolean {
    return event.key === 'Enter' || event.key === ' ';
  }

  private focusStatusAlert(): void {
    queueMicrotask(() => this.statusAlert()?.nativeElement.focus());
  }
}
