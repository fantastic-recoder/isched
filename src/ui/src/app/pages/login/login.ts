import { Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { GraphQLService } from '../../services/graphql.service';
import { AuthService } from '../../services/auth.service';

@Component({
  selector: 'app-login',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  template: `
<div class="min-h-screen flex items-center justify-center bg-base-200 p-4">
  <div class="card w-full max-w-md bg-base-100 shadow-xl">
    <div class="card-body">
      <h2 class="card-title text-2xl mb-1">Sign In</h2>
      <p class="text-sm text-base-content/60 mb-4">Sign in to isched administration.</p>

      @if (errorMsg()) {
        <div class="alert alert-error mb-4">
          <span>{{ errorMsg() }}</span>
        </div>
      }

      <form [formGroup]="form" (ngSubmit)="onSubmit()" novalidate>
        <!-- Email -->
        <div class="form-control mb-3">
          <label class="label" for="email">
            <span class="label-text">Email</span>
          </label>
          <input id="email" type="email" formControlName="email"
                 placeholder="admin@example.com"
                 class="input input-bordered w-full"
                 [class.input-error]="email.invalid && email.touched" />
          @if (email.touched && email.hasError('required')) {
            <p class="text-error text-xs mt-1">Email is required.</p>
          } @else if (email.touched && email.hasError('email')) {
            <p class="text-error text-xs mt-1">Must be a valid email address.</p>
          }
        </div>

        <!-- Password -->
        <div class="form-control mb-6">
          <label class="label" for="password">
            <span class="label-text">Password</span>
          </label>
          <div class="relative">
            <input id="password"
                   [type]="showPw() ? 'text' : 'password'"
                   formControlName="password"
                   placeholder="••••••••••••"
                   class="input input-bordered w-full pr-12"
                   [class.input-error]="pass.invalid && pass.touched" />
            <button type="button" class="absolute inset-y-0 right-3 flex items-center text-base-content/50"
                    (click)="showPw.set(!showPw())" [attr.aria-label]="showPw() ? 'Hide password' : 'Show password'">
              {{ showPw() ? '🙈' : '👁' }}
            </button>
          </div>
          @if (pass.touched && pass.hasError('required')) {
            <p class="text-error text-xs mt-1">Password is required.</p>
          }
        </div>

        <button type="submit" class="btn btn-primary w-full" [disabled]="pending()">
          @if (pending()) {
            <span class="loading loading-spinner loading-sm"></span>
          } @else {
            Sign In
          }
        </button>
      </form>
    </div>
  </div>
</div>
  `,
})
export class LoginComponent {
  private readonly fb     = inject(FormBuilder);
  private readonly gql    = inject(GraphQLService);
  private readonly auth   = inject(AuthService);
  private readonly router = inject(Router);

  readonly form = this.fb.group({
    email:    ['', [Validators.required, Validators.email]],
    password: ['', Validators.required],
  });

  readonly showPw   = signal(false);
  readonly pending  = signal(false);
  readonly errorMsg = signal<string | null>(null);

  get email() { return this.form.controls.email; }
  get pass()  { return this.form.controls.password; }

  onSubmit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || this.pending()) return;

    this.pending.set(true);
    this.errorMsg.set(null);

    const { email, password } = this.form.getRawValue();
    this.gql
      .mutate<{ login: { token: string; expiresAt: string } }>(
        `mutation($email: String!, $password: String!) {
           login(email: $email, password: $password) { token expiresAt }
         }`,
        { email, password },
      )
      .subscribe({
        next: (res) => {
          this.pending.set(false);
          this.auth.setToken(res.login.token);
          void this.router.navigate(['/dashboard']);
        },
        error: (err: Error) => {
          this.pending.set(false);
          this.errorMsg.set(err.message);
        },
      });
  }
}

