import { ChangeDetectionStrategy, Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { OrgContextService } from '../../services/org-context.service';

@Component({
  selector: 'app-users-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './users.page.html',
  styleUrl: './users.page.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class UsersPage {
  private readonly fb = inject(FormBuilder);
  readonly orgContext = inject(OrgContextService);

  readonly warning = signal<string | null>(null);
  readonly form = this.fb.nonNullable.group({
    loginId: ['', [Validators.required, Validators.minLength(2)]],
    displayName: ['', [Validators.required]],
  });

  onOrgChange(nextOrgId: string): void {
    if (!this.orgContext.canSwitchOrganization()) {
      this.warning.set('You have unsaved changes. Save or discard before switching organization.');
      return;
    }
    this.warning.set(null);
    this.orgContext.setOrganization(nextOrgId);
  }

  markDirty(): void {
    this.orgContext.beginDirtyForm();
  }

  markSaved(): void {
    this.orgContext.clearDirtyForm();
  }
}

