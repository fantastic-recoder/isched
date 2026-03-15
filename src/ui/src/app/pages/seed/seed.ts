import { Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators, AbstractControl, ValidationErrors } from '@angular/forms';
import { Router } from '@angular/router';
import { GraphQLService } from '../../services/graphql.service';

function passwordsMatch(ctrl: AbstractControl): ValidationErrors | null {
  const pw = ctrl.get('password')?.value as string | undefined;
  const confirm = ctrl.get('confirmPassword')?.value as string | undefined;
  return pw && confirm && pw !== confirm ? { passwordMismatch: true } : null;
}

@Component({
  selector: 'app-seed',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  template: `
<div class="min-h-screen flex items-center justify-center bg-base-200 p-4">
  <div class="card w-full max-w-md bg-base-100 shadow-xl">
    <div class="card-body">
      <h2 class="card-title text-2xl mb-1">Create Platform Administrator</h2>
      <p class="text-sm text-base-content/60 mb-4">
        First-run setup — no administrator account exists yet.
      </p>

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
        <div class="form-control mb-3">
          <label class="label" for="password">
            <span class="label-text">Password <span class="text-base-content/40">(min 12 characters)</span></span>
          </label>
          <div class="relative">
            <input id="password"
                   [type]="showPw() ? 'text' : 'password'"
                   formControlName="password"
                   placeholder="••••••••••••"
                   class="input input-bordered w-full pr-12"
                   [class.input-error]="password.invalid && password.touched" />
            <button type="button" class="absolute inset-y-0 right-3 flex items-center text-base-content/50"
                    (click)="showPw.set(!showPw())" [attr.aria-label]="showPw() ? 'Hide password' : 'Show password'">
              {{ showPw() ? '🙈' : '👁' }}
            </button>
          </div>
          @if (password.touched && password.hasError('required')) {
            <p class="text-error text-xs mt-1">Password is required.</p>
          } @else if (password.touched && password.hasError('minlength')) {
            <p class="text-error text-xs mt-1">Password must be at least 12 characters.</p>
          }
        </div>

        <!-- Confirm Password -->
        <div class="form-control mb-6">
          <label class="label" for="confirmPassword">
            <span class="label-text">Confirm Password</span>
          </label>
          <div class="relative">
            <input id="confirmPassword"
                   [type]="showConfirmPw() ? 'text' : 'password'"
                   formControlName="confirmPassword"
                   placeholder="••••••••••••"
                   class="input input-bordered w-full pr-12"
                   [class.input-error]="confirmPassword.touched && form.hasError('passwordMismatch')" />
            <button type="button" class="absolute inset-y-0 right-3 flex items-center text-base-content/50"
                    (click)="showConfirmPw.set(!showConfirmPw())" [attr.aria-label]="showConfirmPw() ? 'Hide' : 'Show'">
              {{ showConfirmPw() ? '🙈' : '👁' }}
            </button>
          </div>
          @if (confirmPassword.touched && form.hasError('passwordMismatch')) {
            <p class="text-error text-xs mt-1">Passwords do not match.</p>
          }
        </div>

        <button type="submit" class="btn btn-primary w-full"
                [disabled]="pending()">
          @if (pending()) {
            <span class="loading loading-spinner loading-sm"></span>
          } @else {
            Create Administrator Account
          }
        </button>
      </form>
    </div>
  </div>
</div>
  `,
})
export class SeedComponent {
  private readonly fb    = inject(FormBuilder);
  private readonly gql   = inject(GraphQLService);
  private readonly router = inject(Router);

  readonly form = this.fb.group(
    {
      email:           ['', [Validators.required, Validators.email]],
      password:        ['', [Validators.required, Validators.minLength(12)]],
      confirmPassword: ['', Validators.required],
    },
    { validators: passwordsMatch },
  );

  readonly showPw        = signal(false);
  readonly showConfirmPw = signal(false);
  readonly pending       = signal(false);
  readonly errorMsg      = signal<string | null>(null);

  get email()           { return this.form.controls.email; }
  get password()        { return this.form.controls.password; }
  get confirmPassword() { return this.form.controls.confirmPassword; }

  onSubmit(): void {
    if (this.form.invalid || this.pending()) return;
    this.form.markAllAsTouched();
    if (this.form.invalid) return;

    this.pending.set(true);
    this.errorMsg.set(null);

    const { email, password } = this.form.getRawValue();
    this.gql
      .mutate<{ createPlatformAdmin: { id: string; email: string } }>(
        `mutation($email: String!, $password: String!) {
           createPlatformAdmin(email: $email, password: $password) { id email }
         }`,
        { email, password },
      )
      .subscribe({
        next: () => {
          this.pending.set(false);
          void this.router.navigate(['/login']);
        },
        error: (err: Error) => {
          this.pending.set(false);
          this.errorMsg.set(err.message);
        },
      });
  }
}

