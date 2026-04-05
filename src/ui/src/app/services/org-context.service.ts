import { Injectable, signal } from '@angular/core';

export type OrgContextGuardFailure = {
  ok: false;
  code: 'CONTEXT_MISMATCH' | 'VALIDATION_FAILED';
  message: string;
};

export type OrgContextGuardResult = { ok: true } | OrgContextGuardFailure;

@Injectable({ providedIn: 'root' })
export class OrgContextService {
  readonly selectedOrganizationId = signal<string | null>(null);
  readonly dirtyFormGuardActive = signal(false);

  setOrganization(organizationId: string | null): void {
    this.selectedOrganizationId.set(organizationId);
  }

  clearOrganization(): void {
    this.selectedOrganizationId.set(null);
  }

  beginDirtyForm(): void {
    this.dirtyFormGuardActive.set(true);
  }

  clearDirtyForm(): void {
    this.dirtyFormGuardActive.set(false);
  }

  canSwitchOrganization(): boolean {
    return !this.dirtyFormGuardActive();
  }

  requireSelectedOrganizationId(): string {
    const selected = this.selectedOrganizationId();
    if (!selected) {
      throw new Error('VALIDATION_FAILED: select an organization before performing this action');
    }
    return selected;
  }

  validateMutationScope(requestedOrganizationId: string | null | undefined): OrgContextGuardResult {
    const selected = this.selectedOrganizationId();
    if (!selected) {
      return {
        ok: false,
        code: 'VALIDATION_FAILED',
        message: 'Select an organization before sending organization-scoped changes.',
      };
    }
    if (!requestedOrganizationId || requestedOrganizationId !== selected) {
      return {
        ok: false,
        code: 'CONTEXT_MISMATCH',
        message: 'The selected organization does not match the mutation scope. Refresh context and retry.',
      };
    }
    return { ok: true };
  }
}
