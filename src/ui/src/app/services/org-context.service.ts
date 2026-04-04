import { Injectable, signal } from '@angular/core';

@Injectable({ providedIn: 'root' })
export class OrgContextService {
  readonly selectedOrganizationId = signal<string | null>(null);
  readonly dirtyFormGuardActive = signal(false);

  setOrganization(organizationId: string | null): void {
    this.selectedOrganizationId.set(organizationId);
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
}

