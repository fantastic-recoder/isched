import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component, ElementRef, inject, signal, viewChild } from '@angular/core';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { finalize } from 'rxjs';
import {
  FilterInput,
  Organization,
  OrganizationService,
  PageInfo,
  SortInput,
} from '../../services/organization.service';
import { GraphQLRequestError, GRAPHQL_ERROR_CODES } from '../../services/graphql.service';

const EMPTY_PAGE_INFO: PageInfo = {
  number: 1,
  size: 10,
  totalElements: 0,
  totalPages: 0,
};

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

  readonly statusAlert = viewChild<ElementRef<HTMLElement>>('statusAlert');

  readonly organizations = signal<Organization[]>([]);
  readonly pageInfo = signal<PageInfo>(EMPTY_PAGE_INFO);
  readonly loading = signal(false);
  readonly submitting = signal(false);
  readonly error = signal<string | null>(null);
  readonly statusMessage = signal<string | null>(null);
  readonly editingOrganizationId = signal<string | null>(null);
  readonly editingRevision = signal<number | null>(null);
  readonly createForm = this.fb.nonNullable.group({
    name: ['', [Validators.required, Validators.minLength(2)]],
  });
  readonly filterForm = this.fb.nonNullable.group({
    search: [''],
  });

  constructor() {
    this.loadOrganizations();
  }

  submitOrganization(): void {
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

    this.clearFormServerErrors();
    this.error.set(null);
    this.statusMessage.set(null);
    this.submitting.set(true);

    const editingOrganizationId = this.editingOrganizationId();
    const request$ = editingOrganizationId
      ? this.organizationService.updateOrganization(
          editingOrganizationId,
          { name: trimmedName },
          this.editingRevision() ?? 0,
        )
      : this.organizationService.createOrganization({ name: trimmedName });

    request$
      .pipe(finalize(() => this.submitting.set(false)))
      .subscribe({
        next: (organization) => {
          this.upsertOrganization(organization);
          this.pageInfo.update((page) => ({
            ...page,
            totalElements: editingOrganizationId ? page.totalElements : page.totalElements + 1,
            totalPages: Math.max(page.totalPages, 1),
          }));
          this.resetForm();
          this.statusMessage.set(
            editingOrganizationId
              ? `Updated organization ${organization.name}.`
              : `Created organization ${organization.name}.`,
          );
          this.focusStatusAlert();
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

  editOrganization(orgId: string): void {
    const organization = this.organizations().find((item) => item.id === orgId);
    if (!organization) {
      return;
    }

    this.error.set(null);
    this.clearFormServerErrors();
    this.editingOrganizationId.set(organization.id);
    this.editingRevision.set(organization.revision);
    this.createForm.controls.name.setValue(organization.name);
    this.statusMessage.set(`Editing organization ${organization.name}.`);
    this.focusStatusAlert();
  }

  toggleOrganizationStatus(orgId: string): void {
    const organization = this.organizations().find((item) => item.id === orgId);
    if (!organization) {
      return;
    }

    const nextStatus: Organization['status'] =
      organization.status === 'ACTIVE' ? 'SUSPENDED' : 'ACTIVE';

    this.error.set(null);
    this.submitting.set(true);
    this.organizationService
      .updateOrganization(organization.id, { status: nextStatus }, organization.revision)
      .pipe(finalize(() => this.submitting.set(false)))
      .subscribe({
        next: (updatedOrganization) => {
          this.upsertOrganization(updatedOrganization);
          this.statusMessage.set(
            `${updatedOrganization.name} is now ${updatedOrganization.status.toLowerCase()}.`,
          );
          this.focusStatusAlert();
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

  cancelEdit(): void {
    this.resetForm();
  }

  applySearch(): void {
    this.loadOrganizations(1);
  }

  previousPage(): void {
    const currentPage = this.pageInfo().number;
    if (currentPage <= 1) {
      return;
    }

    this.loadOrganizations(currentPage - 1);
  }

  nextPage(): void {
    const currentPage = this.pageInfo();
    if (currentPage.totalPages <= currentPage.number) {
      return;
    }

    this.loadOrganizations(currentPage.number + 1);
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

  private loadOrganizations(pageNumber = this.pageInfo().number): void {
    this.loading.set(true);
    this.error.set(null);
    this.organizationService
      .listOrganizations({
        page: { number: pageNumber, size: this.pageInfo().size },
        sort: this.sorting(),
        filter: this.filtering(),
      })
      .pipe(finalize(() => this.loading.set(false)))
      .subscribe({
        next: ({ nodes, pageInfo }) => {
          this.organizations.set(nodes);
          this.pageInfo.set(pageInfo);
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

  private sorting(): SortInput[] {
    return [{ field: 'name', direction: 'ASC' }];
  }

  private filtering(): FilterInput[] | undefined {
    const searchTerm = this.filterForm.controls.search.value.trim();
    return searchTerm ? [{ field: 'name', op: 'contains', value: searchTerm }] : undefined;
  }

  private upsertOrganization(organization: Organization): void {
    const existingIndex = this.organizations().findIndex((item) => item.id === organization.id);
    if (existingIndex === -1) {
      this.organizations.update((current) => [organization, ...current]);
      return;
    }

    this.organizations.update((current) =>
      current.map((item) => (item.id === organization.id ? organization : item)),
    );
  }

  private resetForm(): void {
    this.editingOrganizationId.set(null);
    this.editingRevision.set(null);
    this.clearFormServerErrors();
    this.createForm.reset({ name: '' });
  }

  private clearFormServerErrors(): void {
    const currentErrors = this.createForm.controls.name.errors;
    if (!currentErrors?.['server']) {
      return;
    }

    const { server, ...remainingErrors } = currentErrors;
    void server;
    this.createForm.controls.name.setErrors(Object.keys(remainingErrors).length ? remainingErrors : null);
  }

  private handleError(err: unknown): void {
    if (err instanceof GraphQLRequestError) {
      switch (err.code) {
        case GRAPHQL_ERROR_CODES.VALIDATION_FAILED:
          this.error.set('Please correct the highlighted fields and try again.');
          if (err.fieldErrors['name']?.length) {
            this.createForm.controls.name.setErrors({
              ...(this.createForm.controls.name.errors ?? {}),
              server: err.fieldErrors['name'][0],
            });
          }
          return;
        case GRAPHQL_ERROR_CODES.CONFLICT:
          this.error.set('Another administrator changed this organization. Refresh the list and retry your edit.');
          return;
        case GRAPHQL_ERROR_CODES.FORBIDDEN:
          this.error.set('You do not have permission to manage organizations in the current scope.');
          return;
        case GRAPHQL_ERROR_CODES.UNAUTHENTICATED:
          this.error.set('Your session expired. Sign in again and retry the organization change.');
          return;
        default:
          this.error.set(err.message);
          return;
      }
    }

    this.error.set(err instanceof Error ? err.message : 'Organization request failed.');
  }

  private focusStatusAlert(): void {
    queueMicrotask(() => this.statusAlert()?.nativeElement.focus());
  }
}
