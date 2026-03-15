import { Component, inject, signal, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { GraphQLService } from '../../services/graphql.service';
import { AuthService } from '../../services/auth.service';

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

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  template: `
<div class="min-h-screen bg-base-200">
  <!-- Navbar -->
  <div class="navbar bg-base-100 shadow">
    <div class="flex-1">
      <span class="text-xl font-bold px-2">isched</span>
      @if (version()) {
        <span class="text-sm text-base-content/50 ml-2">v{{ version() }}</span>
      }
      @if (health()) {
        @switch (health()) {
          @case ('UP') {
            <span class="badge badge-success badge-sm ml-3">Healthy</span>
          }
          @case ('DOWN') {
            <span class="badge badge-error badge-sm ml-3">Down</span>
          }
          @default {
            <span class="badge badge-warning badge-sm ml-3">{{ health() }}</span>
          }
        }
      }
    </div>
    <div class="flex-none">
      <button class="btn btn-ghost btn-sm" (click)="signOut()">Sign out</button>
    </div>
  </div>

  <main class="container mx-auto p-6">
    <div class="flex justify-between items-center mb-6">
      <h1 class="text-2xl font-bold">Organizations</h1>
      <button class="btn btn-primary btn-sm" onclick="create_org_modal.showModal()">
        + Create Organization
      </button>
    </div>

    @if (loading()) {
      <div class="flex justify-center py-12">
        <span class="loading loading-spinner loading-lg"></span>
      </div>
    } @else if (orgs().length === 0) {
      <div class="alert">No organizations yet. Create one to get started.</div>
    } @else {
      <div class="overflow-x-auto">
        <table class="table table-zebra w-full">
          <thead>
            <tr>
              <th>Name</th>
              <th>Tier</th>
              <th>Users</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            @for (org of orgs(); track org.id) {
              <tr>
                <td>{{ org.name }}</td>
                <td><span class="badge badge-ghost badge-sm">{{ org.subscriptionTier || '—' }}</span></td>
                <td>
                  <div class="collapse collapse-arrow bg-base-200 rounded-box">
                    <input type="checkbox" class="peer" />
                    <div class="collapse-title text-sm font-medium">
                      Show users
                    </div>
                    <div class="collapse-content">
                      @if (users()[org.id] === undefined) {
                        <span class="loading loading-spinner loading-xs"></span>
                      } @else if (users()[org.id].length === 0) {
                        <p class="text-sm text-base-content/50">No users.</p>
                      } @else {
                        <table class="table table-xs w-full mt-1">
                          <thead><tr><th>Email</th><th>Roles</th><th>Active</th></tr></thead>
                          <tbody>
                            @for (u of users()[org.id]; track u.id) {
                              <tr>
                                <td>{{ u.email }}</td>
                                <td>{{ u.roles.join(', ') || '—' }}</td>
                                <td>
                                  @if (u.isActive) {
                                    <span class="badge badge-success badge-xs">Yes</span>
                                  } @else {
                                    <span class="badge badge-ghost badge-xs">No</span>
                                  }
                                </td>
                              </tr>
                            }
                          </tbody>
                        </table>
                      }
                      <button class="btn btn-xs btn-outline mt-2"
                              (click)="loadUsers(org.id)">
                        Refresh
                      </button>
                    </div>
                  </div>
                </td>
                <td>
                  <button class="btn btn-xs btn-secondary"
                          (click)="openCreateUser(org.id)"
                          onclick="create_user_modal.showModal()">
                    + User
                  </button>
                </td>
              </tr>
            }
          </tbody>
        </table>
      </div>
    }
  </main>
</div>

<!-- Create Organization Modal -->
<dialog id="create_org_modal" class="modal">
  <div class="modal-box">
    <h3 class="font-bold text-lg mb-4">Create Organization</h3>
    @if (orgError()) {
      <div class="alert alert-error mb-3 text-sm">{{ orgError() }}</div>
    }
    <form [formGroup]="orgForm" (ngSubmit)="createOrg()" novalidate>
      <div class="form-control mb-2">
        <label class="label"><span class="label-text">Name *</span></label>
        <input type="text" formControlName="name" class="input input-bordered input-sm"
               placeholder="Acme Corp" />
      </div>
      <div class="form-control mb-2">
        <label class="label"><span class="label-text">Domain</span></label>
        <input type="text" formControlName="domain" class="input input-bordered input-sm"
               placeholder="acme.example.com" />
      </div>
      <div class="form-control mb-4">
        <label class="label"><span class="label-text">Subscription Tier</span></label>
        <select formControlName="subscriptionTier" class="select select-bordered select-sm">
          <option value="">— none —</option>
          <option value="free">free</option>
          <option value="pro">pro</option>
          <option value="enterprise">enterprise</option>
        </select>
      </div>
      <div class="modal-action">
        <button type="button" class="btn btn-ghost btn-sm"
                onclick="create_org_modal.close()">Cancel</button>
        <button type="submit" class="btn btn-primary btn-sm" [disabled]="orgPending()">
          @if (orgPending()) { <span class="loading loading-spinner loading-xs"></span> }
          @else { Create }
        </button>
      </div>
    </form>
  </div>
  <form method="dialog" class="modal-backdrop"><button>close</button></form>
</dialog>

<!-- Create User Modal -->
<dialog id="create_user_modal" class="modal">
  <div class="modal-box">
    <h3 class="font-bold text-lg mb-4">Create User</h3>
    @if (userError()) {
      <div class="alert alert-error mb-3 text-sm">{{ userError() }}</div>
    }
    <form [formGroup]="userForm" (ngSubmit)="createUser()" novalidate>
      <div class="form-control mb-2">
        <label class="label"><span class="label-text">Email *</span></label>
        <input type="email" formControlName="email" class="input input-bordered input-sm"
               placeholder="user@example.com" />
      </div>
      <div class="form-control mb-2">
        <label class="label">
          <span class="label-text">Password * <span class="text-base-content/40">(min 12)</span></span>
        </label>
        <div class="relative">
          <input [type]="showUserPw() ? 'text' : 'password'"
                 formControlName="password"
                 class="input input-bordered input-sm w-full pr-10"
                 placeholder="••••••••••••" />
          <button type="button" class="absolute inset-y-0 right-2 flex items-center text-xs"
                  (click)="showUserPw.set(!showUserPw())">
            {{ showUserPw() ? '🙈' : '👁' }}
          </button>
        </div>
        @if (userForm.controls.password.touched && userForm.controls.password.hasError('minlength')) {
          <p class="text-error text-xs mt-1">At least 12 characters required.</p>
        }
      </div>
      <div class="form-control mb-4">
        <label class="label"><span class="label-text">Display Name</span></label>
        <input type="text" formControlName="displayName" class="input input-bordered input-sm"
               placeholder="Jane Doe" />
      </div>
      <div class="modal-action">
        <button type="button" class="btn btn-ghost btn-sm"
                onclick="create_user_modal.close()">Cancel</button>
        <button type="submit" class="btn btn-primary btn-sm" [disabled]="userPending()">
          @if (userPending()) { <span class="loading loading-spinner loading-xs"></span> }
          @else { Create User }
        </button>
      </div>
    </form>
  </div>
  <form method="dialog" class="modal-backdrop"><button>close</button></form>
</dialog>
  `,
})
export class DashboardComponent implements OnInit {
  private readonly fb     = inject(FormBuilder);
  private readonly gql    = inject(GraphQLService);
  private readonly auth   = inject(AuthService);
  private readonly router = inject(Router);

  readonly loading    = signal(true);
  readonly orgs       = signal<Organization[]>([]);
  readonly users      = signal<Record<string, User[]>>({});
  readonly health     = signal<string | null>(null);
  readonly version    = signal<string | null>(null);
  readonly showUserPw = signal(false);

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

  ngOnInit(): void {
    this.loadServerInfo();
    this.loadOrgs();
  }

  private loadServerInfo(): void {
    this.gql
      .query<{ health: { status: string }; version: string }>(
        '{ health { status } version }',
      )
      .subscribe({
        next: (res) => {
          this.health.set(res.health?.status ?? null);
          this.version.set(res.version ?? null);
        },
        error: () => {},
      });
  }

  private loadOrgs(): void {
    this.loading.set(true);
    this.gql
      .query<{ organizations: Organization[] }>(
        '{ organizations { id name subscriptionTier } }',
      )
      .subscribe({
        next: (res) => {
          this.orgs.set(res.organizations ?? []);
          this.loading.set(false);
          // Eagerly load users for the first org if there is one
          for (const org of this.orgs()) this.loadUsers(org.id);
        },
        error: () => this.loading.set(false),
      });
  }

  loadUsers(orgId: string): void {
    this.gql
      .query<{ users: User[] }>(
        `query($id: ID!) { users(organizationId: $id) { id email roles isActive } }`,
        { id: orgId },
      )
      .subscribe({
        next: (res) => {
          this.users.update((prev) => ({ ...prev, [orgId]: res.users ?? [] }));
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
      .mutate<{ createOrganization: Organization }>(
        `mutation($name: String!, $domain: String, $tier: String) {
           createOrganization(input: { name: $name, domain: $domain, subscriptionTier: $tier }) { id name subscriptionTier }
         }`,
        { name, domain: domain || null, tier: subscriptionTier || null },
      )
      .subscribe({
        next: (res) => {
          this.orgPending.set(false);
          this.orgs.update((prev) => [...prev, res.createOrganization]);
          (document.getElementById('create_org_modal') as HTMLDialogElement)?.close();
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
      .mutate<{ createUser: User }>(
        `mutation($orgId: ID!, $email: String!, $pw: String!, $dn: String) {
           createUser(organizationId: $orgId, input: { email: $email, password: $pw, displayName: $dn }) { id email roles isActive }
         }`,
        { orgId: this.selectedOrgId, email, pw: password, dn: displayName || null },
      )
      .subscribe({
        next: () => {
          this.userPending.set(false);
          this.loadUsers(this.selectedOrgId);
          (document.getElementById('create_user_modal') as HTMLDialogElement)?.close();
          this.userForm.reset();
        },
        error: (err: Error) => {
          this.userPending.set(false);
          this.userError.set(err.message);
        },
      });
  }

  signOut(): void {
    if (!confirm('Sign out of isched?')) return;
    this.auth.logout();
    void this.router.navigate(['/login']);
  }
}

