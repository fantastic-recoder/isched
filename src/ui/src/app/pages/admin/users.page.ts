import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component, ElementRef, inject, signal, viewChild } from '@angular/core';
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

  readonly warningAlert = viewChild<ElementRef<HTMLElement>>('warningAlert');
  readonly statusAlert = viewChild<ElementRef<HTMLElement>>('statusAlert');
  readonly loginIdInput = viewChild<ElementRef<HTMLInputElement>>('loginIdInput');

  readonly warning = signal<string | null>(null);
  readonly statusMessage = signal<string | null>(null);
  readonly userActive = signal(true);
  readonly form = this.fb.nonNullable.group({
    loginId: ['', [Validators.required, Validators.minLength(2)]],
    displayName: ['', [Validators.required]],
  });

  onOrgChange(nextOrgId: string): void {
    if (!this.orgContext.canSwitchOrganization()) {
      this.warning.set('You have unsaved changes. Save or discard before switching organization.');
      this.focusWarningAlert();
      return;
    }

    this.warning.set(null);
    this.statusMessage.set(null);
    this.orgContext.setOrganization(nextOrgId);
    this.focusLoginIdInput();
  }

  markDirty(): void {
    this.orgContext.beginDirtyForm();
  }

  markSaved(): void {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      return;
    }

    this.warning.set(null);
    this.statusMessage.set('User changes saved.');
    this.orgContext.clearDirtyForm();
    this.focusStatusAlert();
  }

  deactivateSelectedUser(): void {
    this.userActive.set(false);
    this.statusMessage.set('User marked inactive.');
    this.focusStatusAlert();
  }

  reactivateSelectedUser(): void {
    this.userActive.set(true);
    this.statusMessage.set('User reactivated.');
    this.focusStatusAlert();
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
