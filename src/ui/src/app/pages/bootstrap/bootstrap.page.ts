import { ChangeDetectionStrategy, Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { BootstrapService } from '../../services/bootstrap.service';
import { AuthService } from '../../services/auth.service';
import { mapBootstrapErrorToAlert } from '../../services/auth-alert.mapper';
import {
  INITIAL_BOOTSTRAP_ELIGIBILITY_STATE,
  UserFacingAlert,
} from '../../services/auth-bootstrap.models';
import { GraphQLRequestError, GRAPHQL_ERROR_CODES } from '../../services/graphql.service';
import { SessionBootstrapStateService } from '../../services/session-bootstrap-state.service';

@Component({
  selector: 'app-bootstrap-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './bootstrap.page.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class BootstrapPage implements OnInit {
  private readonly fb = inject(FormBuilder);
  private readonly bootstrapService = inject(BootstrapService);
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);
  private readonly sessionBootstrapState = inject(SessionBootstrapStateService);

  readonly pending = signal(false);
  readonly globalError = signal<string | null>(null);
  readonly bootstrapUnavailableNotice = signal<UserFacingAlert | null>(null);
  readonly recoveryNotice = signal<UserFacingAlert | null>(null);
  readonly showPw = signal(false);

  readonly form = this.fb.nonNullable.group({
    email: ['', [Validators.required, Validators.email]],
    displayName: ['', [Validators.required, Validators.minLength(2)]],
    password: ['', [Validators.required, Validators.minLength(12)]],
  });

  get email() { return this.form.controls.email; }
  get displayName() { return this.form.controls.displayName; }
  get password() { return this.form.controls.password; }

  ngOnInit(): void {
    this.redirectIfBootstrapAlreadyUnavailable();
  }

  submit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || !this.beginBootstrapFlight()) {
      return;
    }

    this.clearSubmitState();

    if (this.redirectIfBootstrapAlreadyUnavailable()) {
      this.pending.set(false);
      return;
    }

    const formData = this.form.getRawValue();

    this.bootstrapService.completeBootstrap(formData).subscribe({
      next: () => {
        this.auth.signIn(formData.email, formData.password).subscribe({
          next: (isAuthenticated) => {
            this.pending.set(false);
            if (isAuthenticated) {
              void this.router.navigate(['/dashboard']);
              return;
            }

            this.redirectToLoginWithRecoveryNotice(formData.email);
          },
          error: () => {
            this.pending.set(false);
            this.redirectToLoginWithRecoveryNotice(formData.email);
          },
        });
      },
      error: (err: unknown) => {
        this.pending.set(false);
        if (this.isBootstrapUnavailableError(err)) {
          this.redirectToLoginForBootstrapUnavailable();
          return;
        }

        if (err instanceof GraphQLRequestError && Object.keys(err.fieldErrors).length > 0) {
          Object.entries(err.fieldErrors).forEach(([field, messages]) => {
            const control = this.form.get(field);
            if (control && messages.length > 0) {
              control.setErrors({ server: messages[0] });
            }
          });
          this.globalError.set(err.message);
          return;
        }

        const alert = mapBootstrapErrorToAlert(err);
        this.globalError.set(alert.body);
      },
    });
  }

  private beginBootstrapFlight(): boolean {
    if (this.pending()) {
      return false;
    }

    this.pending.set(true);
    return true;
  }

  private clearSubmitState(): void {
    this.globalError.set(null);
    this.bootstrapUnavailableNotice.set(null);
    this.recoveryNotice.set(null);
    this.form.setErrors(null);
  }

  private redirectIfBootstrapAlreadyUnavailable(): boolean {
    const availability = this.sessionBootstrapState.bootstrapEligibilityState();
    const bootstrapAvailabilityKnown =
      availability.checkedAt !== INITIAL_BOOTSTRAP_ELIGIBILITY_STATE.checkedAt;

    if (!bootstrapAvailabilityKnown || availability.isAvailable) {
      return false;
    }

    this.redirectToLoginForBootstrapUnavailable();
    return true;
  }

  private redirectToLoginForBootstrapUnavailable(): void {
    this.globalError.set(null);
    this.auth.clearAuthState();
    this.sessionBootstrapState.markBootstrapAvailability(false, 'ActionProbe');
    this.bootstrapUnavailableNotice.set({
      kind: 'Info',
      category: 'BootstrapUnavailable',
      title: 'Bootstrap already completed',
      body: 'Bootstrap is no longer available. Sign in with an existing platform administrator account to continue.',
      dismissible: true,
    });
    void this.router.navigate(['/login'], {
      queryParams: { notice: 'bootstrap-unavailable' },
      replaceUrl: true,
    });
  }

  private redirectToLoginWithRecoveryNotice(email: string): void {
    this.globalError.set(null);
    this.auth.clearAuthState();
    this.recoveryNotice.set({
      kind: 'Info',
      category: 'Generic',
      title: 'Automatic sign-in did not finish',
      body: 'Bootstrap completed successfully. Sign in with the administrator credentials you just created to continue.',
      dismissible: true,
    });
    void this.router.navigate(['/login'], {
      queryParams: {
        notice: 'bootstrap-recovery',
        email,
      },
      replaceUrl: true,
    });
  }

  private isBootstrapUnavailableError(error: unknown): boolean {
    if (typeof error !== 'object' || error === null) {
      return false;
    }

    const maybeError = error as {
      code?: unknown;
      message?: unknown;
    };

    if (maybeError.code === GRAPHQL_ERROR_CODES.CONFLICT) {
      return true;
    }

    return typeof maybeError.message === 'string'
      && maybeError.message.toLowerCase().includes('no longer available');
  }
}

