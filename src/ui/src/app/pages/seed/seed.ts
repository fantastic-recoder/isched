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
  templateUrl: './seed.html',
  styleUrl: './seed.scss',
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

