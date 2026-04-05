import { ChangeDetectionStrategy, Component, inject, signal } from '@angular/core';
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

  readonly roles = signal<RoleRecord[]>([]);
  readonly globalError = signal<string | null>(null);

  readonly createRoleForm = this.fb.nonNullable.group({
    id: ['', [Validators.required]],
    name: ['', [Validators.required]],
    scope: ['tenant', [Validators.required]],
  });

  constructor() {
    this.rbacService.listRoles().subscribe({
      next: ({ roles }) => this.roles.set(roles),
      error: (err: Error) => this.globalError.set(err.message),
    });
  }
}

