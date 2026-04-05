import { ChangeDetectionStrategy, Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { BootstrapService } from '../../services/bootstrap.service';
import { AuthService } from '../../services/auth.service';
import { GraphQLRequestError } from '../../services/graphql.service';

@Component({
  selector: 'app-bootstrap-page',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './bootstrap.page.html',
  styleUrl: './bootstrap.page.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class BootstrapPage {
  private readonly fb = inject(FormBuilder);
  private readonly bootstrapService = inject(BootstrapService);
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);

  readonly pending = signal(false);
  readonly globalError = signal<string | null>(null);

  readonly form = this.fb.nonNullable.group({
    email: ['', [Validators.required, Validators.email]],
    displayName: ['', [Validators.required, Validators.minLength(2)]],
    password: ['', [Validators.required, Validators.minLength(12)]],
  });

  submit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || this.pending()) {
      return;
    }

    this.pending.set(true);
    this.globalError.set(null);
    this.form.setErrors(null);

    const formData = this.form.getRawValue();

    this.bootstrapService.completeBootstrap(formData).subscribe({
      next: () => {
        // Bootstrap succeeded — now log in automatically with the same credentials.
        this.auth.signIn(formData.email, formData.password).subscribe({
          next: () => {
            this.pending.set(false);
            void this.router.navigate(['/dashboard']);
          },
          error: () => {
            // Auto-login failed; fall back to the login page so user can retry.
            this.pending.set(false);
            void this.router.navigate(['/login']);
          },
        });
      },
      error: (err: unknown) => {
        this.pending.set(false);
        if (err instanceof GraphQLRequestError && err.fieldErrors) {
          Object.entries(err.fieldErrors).forEach(([field, messages]) => {
            const control = this.form.get(field);
            if (control && messages.length > 0) {
              control.setErrors({ server: messages[0] });
            }
          });
          this.globalError.set(err.message);
          return;
        }
        this.globalError.set(err instanceof Error ? err.message : 'Bootstrap failed.');
      },
    });
  }
}

