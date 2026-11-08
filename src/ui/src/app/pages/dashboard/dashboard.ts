import { ChangeDetectionStrategy, Component, inject, OnInit, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { RouterLink } from '@angular/router';
import { GraphQLService } from '../../services/graphql.service';

interface Organization {
  id: string;
  name: string;
  subscriptionTier: string;
}

interface User {
  id: string;
  email: string;
  roles: string[];
  isActive: boolean;
}

interface PluginRecord {
  id: string;
  kind: string;
  name: string;
  version: string;
  description: string;
  libraryPath: string;
  isLoaded: boolean;
  loadedAt: string;
  unloadedAt: string;
}

interface PluginOperationResult {
  success: boolean;
  pluginId: string | null;
  errors: string[];
}

type DashboardPanelKey = 'overviewHealth' | 'overviewAdmin' | 'organizations' | 'plugins';

const DASHBOARD_PANEL_STATE_KEY = 'isched.dashboard.panelState';
const DASHBOARD_PLUGIN_ACCORDION_KEY = 'isched.dashboard.pluginAccordionState';

const DEFAULT_PANEL_STATE: Record<DashboardPanelKey, boolean> = {
  overviewHealth: true,
  overviewAdmin: true,
  organizations: true,
  plugins: true,
};

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule, RouterLink],
  templateUrl: './dashboard.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DashboardComponent implements OnInit {
  private readonly fb     = inject(FormBuilder);
  private readonly gql    = inject(GraphQLService);

  readonly loading    = signal(true);
  readonly orgs       = signal<Organization[]>([]);
  readonly users      = signal<Record<string, User[]>>({});
  readonly health     = signal<string | null>(null);
  readonly version    = signal<string | null>(null);
  readonly showUserPw = signal(false);
  readonly panelState = signal<Record<DashboardPanelKey, boolean>>(DEFAULT_PANEL_STATE);

  readonly healthBadgeLabel = () => {
    const value = this.health();
    if (!value) {
      return 'Unknown';
    }
    if (value === 'UP') {
      return 'Healthy';
    }
    if (value === 'DOWN') {
      return 'Down';
    }
    return value;
  };

  readonly healthBadgeClass = () => {
    const value = this.health();
    if (value === 'UP') {
      return 'badge-success';
    }
    if (value === 'DOWN') {
      return 'badge-error';
    }
    return 'badge-warning';
  };

  // Org form
  readonly orgForm    = this.fb.group({ name: ['', Validators.required], domain: [''], subscriptionTier: [''] });
  readonly orgPending = signal(false);
  readonly orgError   = signal<string | null>(null);

  // User form
  readonly userForm    = this.fb.group({
    email:       ['', [Validators.required, Validators.email]],
    password:    ['', [Validators.required, Validators.minLength(12)]],
    displayName: [''],
  });
  readonly userPending    = signal(false);
  readonly userError      = signal<string | null>(null);
  private selectedOrgId   = '';

  // Plugins
  readonly pluginsLoading = signal(true);
  readonly plugins = signal<PluginRecord[]>([]);
  readonly selectedPluginId = signal('');
  readonly expandedPluginIds = signal<string[]>([]);
  readonly pluginMutationPending = signal(false);
  readonly pluginMutationError = signal<string | null>(null);
  readonly pluginForm = this.fb.group({
    libraryPath: ['', Validators.required],
    kind: ['request', Validators.required],
    pluginId: [''],
  });

  ngOnInit(): void {
    this.restorePanelState();
    this.restorePluginAccordionState();
    this.loadServerInfo();
    this.loadOrgs();
    this.loadPlugins();
  }

  isPanelExpanded(key: DashboardPanelKey): boolean {
    return this.panelState()[key];
  }

  onPanelAccordionChange(key: DashboardPanelKey, event: Event): void {
    const checked = (event.target as HTMLInputElement | null)?.checked ?? false;
    this.panelState.update((prev) => ({ ...prev, [key]: checked }));
    this.persistPanelState();
  }

  onPluginAccordionChange(pluginId: string, event: Event): void {
    const checked = (event.target as HTMLInputElement | null)?.checked ?? false;
    this.setPluginExpanded(pluginId, checked);
  }

  isPluginExpanded(pluginId: string): boolean {
    return this.expandedPluginIds().includes(pluginId);
  }

  openLoadPluginModal(): void {
    this.pluginForm.reset({ libraryPath: '', kind: 'request', pluginId: '' });
    this.pluginMutationError.set(null);
    this.openDialog('load_plugin_modal');
  }

  cancelLoadPlugin(): void {
    this.closeDialog('load_plugin_modal');
  }

  loadPlugin(): void {
    this.pluginForm.markAllAsTouched();
    if (this.pluginForm.invalid || this.pluginMutationPending()) {
      return;
    }

    this.pluginMutationPending.set(true);
    this.pluginMutationError.set(null);

    const { libraryPath, kind, pluginId } = this.pluginForm.getRawValue();
    const backendKind = this.toBackendPluginKind(kind);

    this.gql
      .mutate<{ isched: { loadPlugin: PluginOperationResult } }>(
        `mutation($input: LoadPluginInput!) {
           isched {
             loadPlugin(input: $input) {
               success
               pluginId
               errors
             }
           }
         }`,
        {
          input: {
            libraryPath,
            kind: backendKind,
            pluginId: pluginId?.trim() ? pluginId.trim() : null,
          },
        },
      )
      .subscribe({
        next: (res) => {
          const result = res.isched?.loadPlugin;
          if (!result?.success) {
            this.pluginMutationPending.set(false);
            this.pluginMutationError.set(result?.errors?.[0] ?? 'Plugin load failed.');
            return;
          }

          this.pluginMutationPending.set(false);
          this.closeDialog('load_plugin_modal');
          this.pluginForm.reset({ libraryPath: '', kind: 'request', pluginId: '' });
          this.loadPlugins(result.pluginId ?? undefined);
        },
        error: (err: Error) => {
          this.pluginMutationPending.set(false);
          this.pluginMutationError.set(err.message);
        },
      });
  }

  unloadSelectedPlugin(): void {
    const pluginId = this.selectedPluginId();
    if (!pluginId || this.pluginMutationPending()) {
      return;
    }

    this.pluginMutationPending.set(true);
    this.pluginMutationError.set(null);

    this.gql
      .mutate<{ isched: { unloadPlugin: PluginOperationResult } }>(
        `mutation($id: ID!) {
           isched {
             unloadPlugin(id: $id) {
               success
               pluginId
               errors
             }
           }
         }`,
        { id: pluginId },
      )
      .subscribe({
        next: (res) => {
          const result = res.isched?.unloadPlugin;
          if (!result?.success) {
            this.pluginMutationPending.set(false);
            this.pluginMutationError.set(result?.errors?.[0] ?? 'Plugin unload failed.');
            return;
          }

          this.pluginMutationPending.set(false);
          this.loadPlugins();
        },
        error: (err: Error) => {
          this.pluginMutationPending.set(false);
          this.pluginMutationError.set(err.message);
        },
      });
  }

  private loadServerInfo(): void {
    this.gql
      .query<{ isched: { health: { status: string } | null; version: string | null } }>(
        '{ isched { health { status } version } }',
      )
      .subscribe({
        next: (res) => {
          this.health.set(res.isched?.health?.status ?? null);
          this.version.set(res.isched?.version ?? null);
        },
        error: () => {},
      });
  }

  private loadOrgs(): void {
    this.loading.set(true);
    this.gql
      .query<{ isched: { organizations: { nodes: Organization[] } } }>(
        '{ isched { organizations { nodes { id name subscriptionTier } } } }',
      )
      .subscribe({
        next: (res) => {
          this.orgs.set(res.isched?.organizations?.nodes ?? []);
          this.loading.set(false);
          // Eagerly load users for the first org if there is one
          for (const org of this.orgs()) this.loadUsers(org.id);
        },
        error: () => this.loading.set(false),
      });
  }

  private loadPlugins(preferredSelectedId?: string): void {
    this.pluginsLoading.set(true);

    this.gql
      .query<{ isched: { plugins: PluginRecord[] } }>(
        `query {
           isched {
             plugins {
               id
               kind
               name
               version
               description
               libraryPath
               isLoaded
               loadedAt
               unloadedAt
             }
           }
         }`,
      )
      .subscribe({
        next: (res) => {
          const list = res.isched?.plugins ?? [];
          this.plugins.set(list);
          this.pluginsLoading.set(false);

          const validIds = new Set(list.map((plugin) => plugin.id));
          this.expandedPluginIds.update((prev) => prev.filter((id) => validIds.has(id)));
          this.persistPluginAccordionState();

          const nextSelectedId = preferredSelectedId && validIds.has(preferredSelectedId)
            ? preferredSelectedId
            : this.selectedPluginId();

          this.selectedPluginId.set(
            nextSelectedId && validIds.has(nextSelectedId) ? nextSelectedId : (list[0]?.id ?? ''),
          );
        },
        error: () => {
          this.pluginsLoading.set(false);
          this.plugins.set([]);
          this.selectedPluginId.set('');
        },
      });
  }

  private setPluginExpanded(pluginId: string, expanded: boolean): void {
    this.expandedPluginIds.update((prev) => {
      const ids = new Set(prev);
      if (expanded) {
        ids.add(pluginId);
      } else {
        ids.delete(pluginId);
      }
      return Array.from(ids);
    });
    this.persistPluginAccordionState();
  }

  private restorePanelState(): void {
    const raw = this.readLocalStorage(DASHBOARD_PANEL_STATE_KEY);
    if (!raw) {
      return;
    }

    try {
      const parsed = JSON.parse(raw) as Partial<Record<DashboardPanelKey, boolean>>;
      this.panelState.set({
        overviewHealth: parsed.overviewHealth ?? DEFAULT_PANEL_STATE.overviewHealth,
        overviewAdmin: parsed.overviewAdmin ?? DEFAULT_PANEL_STATE.overviewAdmin,
        organizations: parsed.organizations ?? DEFAULT_PANEL_STATE.organizations,
        plugins: parsed.plugins ?? DEFAULT_PANEL_STATE.plugins,
      });
    } catch {
      this.panelState.set(DEFAULT_PANEL_STATE);
    }
  }

  private persistPanelState(): void {
    this.writeLocalStorage(DASHBOARD_PANEL_STATE_KEY, JSON.stringify(this.panelState()));
  }

  private restorePluginAccordionState(): void {
    const raw = this.readLocalStorage(DASHBOARD_PLUGIN_ACCORDION_KEY);
    if (!raw) {
      return;
    }

    try {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) {
        this.expandedPluginIds.set(parsed.filter((entry) => typeof entry === 'string'));
      }
    } catch {
      this.expandedPluginIds.set([]);
    }
  }

  private persistPluginAccordionState(): void {
    this.writeLocalStorage(DASHBOARD_PLUGIN_ACCORDION_KEY, JSON.stringify(this.expandedPluginIds()));
  }

  private readLocalStorage(key: string): string | null {
    try {
      return globalThis.localStorage?.getItem(key) ?? null;
    } catch {
      return null;
    }
  }

  private writeLocalStorage(key: string, value: string): void {
    try {
      globalThis.localStorage?.setItem(key, value);
    } catch {
      // Ignore storage write failures (private mode, disabled storage, etc.).
    }
  }

  private openDialog(dialogId: string): void {
    (document.getElementById(dialogId) as HTMLDialogElement | null)?.showModal();
  }

  private closeDialog(dialogId: string): void {
    (document.getElementById(dialogId) as HTMLDialogElement | null)?.close();
  }

  private toBackendPluginKind(kind: string | null | undefined): 'request' | 'resolver' {
    return String(kind ?? '').toLowerCase() === 'resolver' ? 'resolver' : 'request';
  }

  loadUsers(orgId: string): void {
    this.gql
      .query<{ isched: { users: User[] } }>(
        `query($id: ID!) { isched { users(organizationId: $id) { id email roles isActive } } }`,
        { id: orgId },
      )
      .subscribe({
        next: (res) => {
          this.users.update((prev) => ({ ...prev, [orgId]: res.isched?.users ?? [] }));
        },
        error: () => {
          this.users.update((prev) => ({ ...prev, [orgId]: [] }));
        },
      });
  }

  openCreateUser(orgId: string): void {
    this.selectedOrgId = orgId;
    this.userForm.reset();
    this.userError.set(null);
    this.showUserPw.set(false);
  }

  createOrg(): void {
    this.orgForm.markAllAsTouched();
    if (this.orgForm.invalid || this.orgPending()) return;
    this.orgPending.set(true);
    this.orgError.set(null);
    const { name, domain, subscriptionTier } = this.orgForm.getRawValue();
    this.gql
      .mutate<{ isched: { createOrganization: Organization } }>(
        `mutation($name: String!, $domain: String, $tier: String) {
           isched {
             createOrganization(input: { name: $name, domain: $domain, subscriptionTier: $tier }) {
               id name subscriptionTier
             }
           }
         }`,
        { name, domain: domain || null, tier: subscriptionTier || null },
      )
      .subscribe({
        next: (res) => {
          this.orgPending.set(false);
          this.orgs.update((prev) => [...prev, res.isched.createOrganization]);
          this.closeDialog('create_org_modal');
          this.orgForm.reset();
        },
        error: (err: Error) => {
          this.orgPending.set(false);
          this.orgError.set(err.message);
        },
      });
  }

  createUser(): void {
    this.userForm.markAllAsTouched();
    if (this.userForm.invalid || this.userPending()) return;
    this.userPending.set(true);
    this.userError.set(null);
    const { email, password, displayName } = this.userForm.getRawValue();
    this.gql
      .mutate<{ isched: { createUser: User } }>(
        `mutation($orgId: ID!, $email: String!, $pw: String!, $dn: String) {
           isched {
             createUser(organizationId: $orgId, input: { email: $email, password: $pw, displayName: $dn }) {
               id email roles isActive
             }
           }
         }`,
        { orgId: this.selectedOrgId, email, pw: password, dn: displayName || null },
      )
      .subscribe({
        next: () => {
          this.userPending.set(false);
          this.loadUsers(this.selectedOrgId);
          this.closeDialog('create_user_modal');
          this.userForm.reset();
        },
        error: (err: Error) => {
          this.userPending.set(false);
          this.userError.set(err.message);
        },
      });
  }
}
