import { ChangeDetectionStrategy, Component, ElementRef, inject, signal, viewChild } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { RbacService, RoleRecord } from '../../services/rbac.service';

@Component({
  selector: 'app-rbac-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './rbac.page.html',
  styleUrl: './rbac.page.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class RbacPage {
  private readonly rbacService = inject(RbacService);
  private readonly fb = inject(FormBuilder);

  readonly statusAlert = viewChild<ElementRef<HTMLElement>>('statusAlert');
  readonly assignUserInput = viewChild<ElementRef<HTMLInputElement>>('assignUserInput');

  readonly roles = signal<RoleRecord[]>([]);
  readonly globalError = signal<string | null>(null);
  readonly statusMessage = signal<string | null>(null);
  readonly editingRoleId = signal<string | null>(null);

  readonly createRoleForm = this.fb.nonNullable.group({
    id: ['', [Validators.required]],
    name: ['', [Validators.required]],
    scope: ['tenant', [Validators.required]],
  });

  readonly assignmentForm = this.fb.nonNullable.group({
    userId: ['', [Validators.required]],
    roleId: ['', [Validators.required]],
  });

   constructor() {
     this.rbacService.listRoles().subscribe({
       next: ({ roles }) => {
         this.roles.set(roles);
         if (roles.length > 0) {
           this.assignmentForm.controls.roleId.setValue(roles[0].id);
         }
       },
       error: (err: unknown) => this.handleError(err),
     });
   }

  submitRoleForm(): void {
    if (this.createRoleForm.invalid) {
      this.createRoleForm.markAllAsTouched();
      return;
    }

    const roleId = this.createRoleForm.controls.id.value.trim();
    const roleName = this.createRoleForm.controls.name.value.trim();
    const roleScope = this.createRoleForm.controls.scope.value;

    if (!roleId || !roleName) {
      this.createRoleForm.markAllAsTouched();
      return;
    }

    const editingId = this.editingRoleId();
    if (editingId) {
      this.roles.update((current) =>
        current.map((role) =>
          role.id === editingId
            ? {
                ...role,
                name: roleName,
                scope: roleScope,
              }
            : role,
        ),
      );
      this.statusMessage.set(`Updated role ${roleName}.`);
      this.editingRoleId.set(null);
      this.createRoleForm.reset({ id: '', name: '', scope: 'tenant' });
      this.focusStatusAlert();
      return;
    }

    if (this.roles().some((role) => role.id === roleId)) {
      this.globalError.set(`Role ${roleId} already exists.`);
      return;
    }

    this.roles.update((current) => [...current, { id: roleId, name: roleName, scope: roleScope }]);
    this.assignmentForm.controls.roleId.setValue(roleId);
    this.globalError.set(null);
    this.statusMessage.set(`Created role ${roleName}.`);
    this.createRoleForm.reset({ id: '', name: '', scope: 'tenant' });
    this.focusStatusAlert();
  }

  beginEditRole(roleId: string): void {
    const role = this.roles().find((entry) => entry.id === roleId);
    if (!role) {
      return;
    }

    this.editingRoleId.set(roleId);
    this.createRoleForm.setValue({
      id: role.id,
      name: role.name,
      scope: role.scope,
    });
    this.statusMessage.set(`Editing role ${role.name}.`);
    this.focusStatusAlert();
  }

  prepareAssignRole(roleId: string): void {
    this.assignmentForm.controls.roleId.setValue(roleId);
    this.statusMessage.set('Role selected for assignment. Provide a user ID to continue.');
    this.focusAssignUserInput();
  }

  assignRole(): void {
    if (this.assignmentForm.invalid) {
      this.assignmentForm.markAllAsTouched();
      return;
    }

    const userId = this.assignmentForm.controls.userId.value.trim();
    const roleId = this.assignmentForm.controls.roleId.value;
    const role = this.roles().find((entry) => entry.id === roleId);

    if (!role || !userId) {
      this.assignmentForm.markAllAsTouched();
      return;
    }

    this.statusMessage.set(`Assigned role ${role.name} to ${userId}.`);
    this.assignmentForm.controls.userId.reset('');
    this.focusStatusAlert();
  }

  onEditKeydown(event: KeyboardEvent, roleId: string): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.beginEditRole(roleId);
  }

  onAssignKeydown(event: KeyboardEvent, roleId: string): void {
    if (!this.isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    this.prepareAssignRole(roleId);
  }

   private isActivationKey(event: KeyboardEvent): boolean {
     return event.key === 'Enter' || event.key === ' ';
   }

   private handleError(err: unknown): void {
     // Filter out URL-like error messages that might come from network errors
     const errorMessage = err instanceof Error ? err.message : 'RBAC request failed.';
     if (errorMessage.startsWith('http://') || errorMessage.startsWith('https://')) {
       this.globalError.set('RBAC request failed.');
     } else {
       this.globalError.set(errorMessage);
     }
   }

   private focusStatusAlert(): void {
     queueMicrotask(() => this.statusAlert()?.nativeElement.focus());
   }

   private focusAssignUserInput(): void {
     queueMicrotask(() => this.assignUserInput()?.nativeElement.focus());
   }
}
