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
  templateUrl: './dashboard.html',
  styleUrl: './dashboard.scss',
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
    this.auth.signOut().subscribe({
      next: () => {
        void this.router.navigate(['/login']);
      },
      error: () => {
        void this.router.navigate(['/login']);
      },
    });
  }
}

