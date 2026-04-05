import { Component, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormBuilder, Validators } from '@angular/forms';
import { Router } from '@angular/router';
import { AuthService } from '../../services/auth.service';

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
  readonly errorMsg = signal<string | null>(null);

  get email() { return this.form.controls.email; }
  get pass()  { return this.form.controls.password; }

  onSubmit(): void {
    this.form.markAllAsTouched();
    if (this.form.invalid || this.pending()) return;

    this.pending.set(true);
    this.errorMsg.set(null);

    const { email, password } = this.form.getRawValue();
    this.auth
      .signIn(email ?? '', password ?? '')
      .subscribe({
        next: () => {
          this.pending.set(false);
          void this.router.navigate(['/dashboard']);
        },
        error: (err: Error) => {
          this.pending.set(false);
          this.errorMsg.set(err.message);
        },
      });
  }
}

