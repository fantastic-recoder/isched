import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';
import { AuthService, AuthSignInError } from '../../services/auth.service';
import { UserFacingAlert } from '../../services/auth-bootstrap.models';

@Component({
  selector: 'app-login',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './login.html',
})
export class LoginComponent {
  private readonly fb     = inject(FormBuilder);
  private readonly auth   = inject(AuthService);
  private readonly route  = inject(ActivatedRoute);
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

  ngOnInit(): void {
    this.applyBootstrapNoticeHandoff();
  }

  onSubmit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || !this.beginSignInFlight()) return;

    this.lockoutAlert.set(null);
    this.authAlert.set(null);

    const { email, password } = this.form.getRawValue();
    this.auth
      .signIn(email ?? '', password ?? '')
      .subscribe({
        next: (ok) => {
          this.finishSignInFlight();
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
          this.finishSignInFlight();
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

  private beginSignInFlight(): boolean {
    if (this.pending()) {
      return false;
    }

    this.pending.set(true);
    return true;
  }

  private finishSignInFlight(): void {
    this.pending.set(false);
  }

  private applyBootstrapNoticeHandoff(): void {
    const notice = this.route.snapshot.queryParamMap.get('notice');
    const email = this.route.snapshot.queryParamMap.get('email');

    if (email) {
      this.email.setValue(email);
    }

    switch (notice) {
      case 'bootstrap-unavailable':
        this.auth.clearAuthState();
        this.authAlert.set({
          kind: 'Info',
          category: 'BootstrapUnavailable',
          title: 'Bootstrap already completed',
          body: 'Bootstrap has already been completed. Sign in with an existing platform administrator account to continue.',
          dismissible: true,
        });
        break;
      case 'bootstrap-recovery':
        this.auth.clearAuthState();
        this.authAlert.set({
          kind: 'Info',
          category: 'Generic',
          title: 'Automatic sign-in did not finish',
          body: 'Bootstrap completed successfully. Sign in with the administrator credentials you just created to continue.',
          dismissible: true,
        });
        break;
      default:
        break;
    }
  }

  retryAfterSeconds(retryAfterMs: number | undefined): number | null {
    if (typeof retryAfterMs !== 'number' || retryAfterMs <= 0) {
      return null;
    }
    return Math.max(1, Math.ceil(retryAfterMs / 1000));
  }
}

