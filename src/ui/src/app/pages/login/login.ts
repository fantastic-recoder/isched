import { Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { AuthService, AuthSignInError } from '../../services/auth.service';
import { UserFacingAlert } from '../../services/auth-bootstrap.models';

@Component({
  selector: 'app-login',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './login.html',
  styleUrl: './login.scss',
})
export class LoginComponent {
  private readonly fb     = inject(FormBuilder);
  private readonly auth   = inject(AuthService);
  private readonly router = inject(Router);

  readonly form = this.fb.group({
    email:    ['', [Validators.required, Validators.email]],
    password: ['', Validators.required],
  });

  readonly showPw   = signal(false);
  readonly pending  = signal(false);
  readonly lockoutAlert = signal<UserFacingAlert | null>(null);
  readonly authAlert = signal<UserFacingAlert | null>(null);

  get email() { return this.form.controls.email; }
  get pass()  { return this.form.controls.password; }

  onSubmit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || this.pending()) return;

    this.pending.set(true);
    this.lockoutAlert.set(null);
    this.authAlert.set(null);

    const { email, password } = this.form.getRawValue();
    this.auth
      .signIn(email ?? '', password ?? '')
      .subscribe({
        next: (ok) => {
          this.pending.set(false);
          if (ok) {
            void this.router.navigate(['/dashboard']);
            return;
          }

          this.authAlert.set({
            kind: 'Error',
            category: 'AuthFailure',
            title: 'Sign-in failed',
            body: 'Unable to sign in right now. Please verify your credentials and try again.',
            dismissible: true,
          });
        },
        error: (err: unknown) => {
          this.pending.set(false);
          if (err instanceof AuthSignInError) {
            if (err.alert.category === 'AuthRateLimited') {
              this.lockoutAlert.set(err.alert);
              return;
            }
            this.authAlert.set(err.alert);
            return;
          }

          this.authAlert.set({
            kind: 'Error',
            category: 'Generic',
            title: 'Unexpected error',
            body: err instanceof Error ? err.message : 'Something went wrong. Please try again.',
            dismissible: true,
          });
        },
      });
  }

  retryAfterSeconds(retryAfterMs: number | undefined): number | null {
    if (typeof retryAfterMs !== 'number' || retryAfterMs <= 0) {
      return null;
    }
    return Math.max(1, Math.ceil(retryAfterMs / 1000));
  }
}

