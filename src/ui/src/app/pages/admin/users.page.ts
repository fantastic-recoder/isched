import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component, ElementRef, inject, signal, viewChild } from '@angular/core';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { finalize } from 'rxjs';
import { OrgContextService } from '../../services/org-context.service';
import {
  FilterInput,
  Organization,
  OrganizationService,
  PageInfo,
  SortInput,
} from '../../services/organization.service';
import { GraphQLRequestError, GRAPHQL_ERROR_CODES } from '../../services/graphql.service';
import { UserRecord, UserService } from '../../services/user.service';

const EMPTY_PAGE_INFO: PageInfo = {
  number: 1,
  size: 10,
  totalElements: 0,
  totalPages: 0,
};

@Component({
  selector: 'app-users-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './users.page.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class UsersPage {
  private readonly fb = inject(FormBuilder);
  private readonly organizationService = inject(OrganizationService);
  private readonly userService = inject(UserService);
  readonly orgContext = inject(OrgContextService);

  readonly warningAlert = viewChild<ElementRef<HTMLElement>>('warningAlert');
  readonly statusAlert = viewChild<ElementRef<HTMLElement>>('statusAlert');
  readonly loginIdInput = viewChild<ElementRef<HTMLInputElement>>('loginIdInput');

  readonly organizations = signal<Organization[]>([]);
  readonly users = signal<UserRecord[]>([]);
  readonly pageInfo = signal<PageInfo>(EMPTY_PAGE_INFO);
  readonly error = signal<string | null>(null);
  readonly warning = signal<string | null>(null);
  readonly statusMessage = signal<string | null>(null);
  readonly loading = signal(false);
  readonly submitting = signal(false);
  readonly selectedUserId = signal<string | null>(null);
  readonly selectedUserRevision = signal<number | null>(null);
  readonly form = this.fb.nonNullable.group({
    loginId: ['', [Validators.required, Validators.minLength(2)]],
    displayName: ['', [Validators.required]],
  });
  readonly filterForm = this.fb.nonNullable.group({
    search: [''],
  });

  constructor() {
    this.loadOrganizations();
  }

  onOrgChange(nextOrgId: string): void {
    if (!this.orgContext.canSwitchOrganization()) {
      this.warning.set('You have unsaved changes. Save or discard before switching organization.');
      this.focusWarningAlert();
      return;
    }

    this.warning.set(null);
    this.error.set(null);
    this.statusMessage.set(null);
    this.orgContext.setOrganization(nextOrgId);
    this.resetEditor();
    this.loadUsers(1);
    this.focusLoginIdInput();
  }

  markDirty(): void {
    this.orgContext.beginDirtyForm();
  }

  saveUser(): void {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      return;
    }

    const organizationId = this.orgContext.requireSelectedOrganizationId();
    const loginId = this.form.controls.loginId.value.trim();
    const displayName = this.form.controls.displayName.value.trim();

    if (!loginId) {
      this.form.controls.loginId.setErrors({ required: true });
      this.form.controls.loginId.markAsTouched();
      return;
    }

    if (!displayName) {
      this.form.controls.displayName.setErrors({ required: true });
      this.form.controls.displayName.markAsTouched();
      return;
    }

    this.clearServerErrors();
    this.error.set(null);
    this.warning.set(null);
    this.statusMessage.set(null);
    this.submitting.set(true);

    const selectedUserId = this.selectedUserId();
    const request$ = selectedUserId
      ? this.userService.updateUser(
          organizationId,
          selectedUserId,
          { loginId, displayName },
          this.selectedUserRevision() ?? 0,
        )
      : this.userService.createUser(organizationId, { loginId, displayName });

    request$
      .pipe(finalize(() => this.submitting.set(false)))
      .subscribe({
        next: (user) => {
          this.upsertUser(user);
          this.pageInfo.update((page) => ({
            ...page,
            totalElements: selectedUserId ? page.totalElements : page.totalElements + 1,
            totalPages: Math.max(page.totalPages, 1),
          }));
          this.selectUser(user.id);
          this.statusMessage.set('User changes saved.');
          this.orgContext.clearDirtyForm();
          this.focusStatusAlert();
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

  deactivateSelectedUser(): void {
    this.updateSelectedUserStatus('DISABLED');
  }

  reactivateSelectedUser(): void {
    this.updateSelectedUserStatus('ACTIVE');
  }

  selectedUser(): UserRecord | null {
    const selectedUserId = this.selectedUserId();
    return this.users().find((item) => item.id === selectedUserId) ?? null;
  }

  selectUser(userId: string): void {
    const user = this.users().find((item) => item.id === userId);
    if (!user) {
      return;
    }

    this.error.set(null);
    this.warning.set(null);
    this.clearServerErrors();
    this.selectedUserId.set(user.id);
    this.selectedUserRevision.set(user.revision);
    this.form.setValue({ loginId: user.loginId, displayName: user.displayName });
    this.orgContext.clearDirtyForm();
  }

  resetEditor(): void {
    this.selectedUserId.set(null);
    this.selectedUserRevision.set(null);
    this.clearServerErrors();
    this.form.reset({ loginId: '', displayName: '' });
    this.orgContext.clearDirtyForm();
  }

  applySearch(): void {
    this.loadUsers(1);
  }

  previousPage(): void {
    const currentPage = this.pageInfo().number;
    if (currentPage <= 1) {
      return;
    }

    this.loadUsers(currentPage - 1);
  }

  nextPage(): void {
    const currentPage = this.pageInfo();
    if (currentPage.totalPages <= currentPage.number) {
      return;
    }

    this.loadUsers(currentPage.number + 1);
  }

  onDeactivateKeydown(event: KeyboardEvent): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.deactivateSelectedUser();
  }

  onReactivateKeydown(event: KeyboardEvent): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.reactivateSelectedUser();
  }

  private isActivationKey(event: KeyboardEvent): boolean {
    return event.key === 'Enter' || event.key === ' ';
  }

  private loadOrganizations(): void {
    this.organizationService
      .listOrganizations({
        page: { number: 1, size: 25 },
        sort: [{ field: 'name', direction: 'ASC' }],
      })
      .subscribe({
        next: ({ nodes }) => {
          this.organizations.set(nodes);
          const selectedOrganizationId = this.orgContext.selectedOrganizationId();
          const nextOrganizationId = selectedOrganizationId ?? nodes[0]?.id ?? null;
          if (!nextOrganizationId) {
            return;
          }

          this.orgContext.setOrganization(nextOrganizationId);
          this.loadUsers();
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

   private loadUsers(pageNumber = this.pageInfo().number): void {
     const organizationId = this.orgContext.selectedOrganizationId();
     if (!organizationId) {
       this.users.set([]);
       this.pageInfo.set(EMPTY_PAGE_INFO);
       this.loading.set(false);
       return;
     }

     this.loading.set(true);
     this.error.set(null);
      this.statusMessage.set(null);
     this.userService
       .listUsers({
         organizationId,
         page: { number: pageNumber, size: this.pageInfo().size },
         sort: this.sorting(),
         filter: this.filtering(),
       })
       .pipe(finalize(() => this.loading.set(false)))
       .subscribe({
         next: ({ nodes, pageInfo }) => {
           this.users.set(nodes);
           this.pageInfo.set(pageInfo);
           if (!this.selectedUserId() || !nodes.some((user) => user.id === this.selectedUserId())) {
             if (nodes[0]) {
               this.selectUser(nodes[0].id);
             } else {
               this.resetEditor();
             }
           }
         },
         error: (err: unknown) => this.handleError(err),
       });
   }

  private sorting(): SortInput[] {
    return [{ field: 'displayName', direction: 'ASC' }];
  }

  private filtering(): FilterInput[] | undefined {
    const searchTerm = this.filterForm.controls.search.value.trim();
    return searchTerm
      ? [
          { field: 'loginId', op: 'contains', value: searchTerm },
          { field: 'displayName', op: 'contains', value: searchTerm },
        ]
      : undefined;
  }

  private updateSelectedUserStatus(status: UserRecord['status']): void {
    const selectedUserId = this.selectedUserId();
    const organizationId = this.orgContext.selectedOrganizationId();
    const selectedUser = this.users().find((item) => item.id === selectedUserId);
    if (!selectedUserId || !organizationId || !selectedUser) {
      return;
    }

    this.submitting.set(true);
    this.error.set(null);
    this.userService
      .updateUser(organizationId, selectedUserId, { status }, this.selectedUserRevision() ?? selectedUser.revision)
      .pipe(finalize(() => this.submitting.set(false)))
      .subscribe({
        next: (user) => {
          this.upsertUser(user);
          this.selectUser(user.id);
          this.statusMessage.set(`${user.displayName} is now ${user.status === 'ACTIVE' ? 'active' : 'disabled'}.`);
          this.focusStatusAlert();
        },
        error: (err: unknown) => this.handleError(err),
      });
  }

  private upsertUser(user: UserRecord): void {
    const existingIndex = this.users().findIndex((item) => item.id === user.id);
    if (existingIndex === -1) {
      this.users.update((current) => [user, ...current]);
      return;
    }

    this.users.update((current) => current.map((item) => (item.id === user.id ? user : item)));
  }

  private clearServerErrors(): void {
    for (const control of [this.form.controls.loginId, this.form.controls.displayName]) {
      const currentErrors = control.errors;
      if (!currentErrors?.['server']) {
        continue;
      }

      const { server, ...remainingErrors } = currentErrors;
      void server;
      control.setErrors(Object.keys(remainingErrors).length ? remainingErrors : null);
    }
  }

   private handleError(err: unknown): void {
     if (err instanceof GraphQLRequestError) {
       switch (err.code) {
         case GRAPHQL_ERROR_CODES.VALIDATION_FAILED:
           this.error.set('Please correct the highlighted fields and try again.');
           if (err.fieldErrors['loginId']?.length) {
             this.form.controls.loginId.setErrors({
               ...(this.form.controls.loginId.errors ?? {}),
               server: err.fieldErrors['loginId'][0],
             });
           }
           if (err.fieldErrors['displayName']?.length) {
             this.form.controls.displayName.setErrors({
               ...(this.form.controls.displayName.errors ?? {}),
               server: err.fieldErrors['displayName'][0],
             });
           }
           return;
         case GRAPHQL_ERROR_CODES.CONFLICT:
           this.error.set('Another administrator changed this user. Refresh the current organization and retry your update.');
           return;
         case GRAPHQL_ERROR_CODES.CONTEXT_MISMATCH:
           this.error.set('The selected organization does not match the user mutation scope. Refresh context and retry.');
           return;
         case GRAPHQL_ERROR_CODES.FORBIDDEN:
           this.error.set('You do not have permission to manage users in the current organization.');
           return;
         case GRAPHQL_ERROR_CODES.UNAUTHENTICATED:
           this.error.set('Your session expired. Sign in again and retry the user change.');
           return;
         default:
           this.error.set(err.message);
           return;
       }
     }

     // Filter out URL-like error messages that might come from network errors
     const errorMessage = err instanceof Error ? err.message : 'User request failed.';
     if (errorMessage.startsWith('http://') || errorMessage.startsWith('https://')) {
       this.error.set('User request failed.');
     } else {
       this.error.set(errorMessage);
     }
   }

  private focusWarningAlert(): void {
    queueMicrotask(() => this.warningAlert()?.nativeElement.focus());
  }

  private focusStatusAlert(): void {
    queueMicrotask(() => this.statusAlert()?.nativeElement.focus());
  }

  private focusLoginIdInput(): void {
    queueMicrotask(() => this.loginIdInput()?.nativeElement.focus());
  }
}
