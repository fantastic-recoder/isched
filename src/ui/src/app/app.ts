import { Component, computed, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NavigationEnd, Router, RouterOutlet } from '@angular/router';
import { toSignal } from '@angular/core/rxjs-interop';
import { catchError, filter, firstValueFrom, from, map, of, startWith, switchMap, take } from 'rxjs';
import { AuthService } from './services/auth.service';
import { BootstrapService } from './services/bootstrap.service';
import { SessionBootstrapStateService } from './services/session-bootstrap-state.service';
import { AuthenticatedShellComponent } from './components/authenticated-shell/authenticated-shell.component';

@Component({
  selector: 'app-root',
  imports: [CommonModule, RouterOutlet, AuthenticatedShellComponent],
  templateUrl: './app.html',
  host: { class: 'block min-h-screen' },
})
export class App {
  private readonly auth = inject(AuthService);
  private readonly bootstrapService = inject(BootstrapService);
  private readonly router = inject(Router);
  private readonly sessionBootstrapState = inject(SessionBootstrapStateService);

  private readonly currentUrl = toSignal(
    this.router.events.pipe(
      filter((event): event is NavigationEnd => event instanceof NavigationEnd),
      map((event) => event.urlAfterRedirects),
      startWith(this.router.url),
    ),
    { initialValue: this.router.url },
  );

  private readonly localSeedModeHint = toSignal(this.bootstrapService.seedModeHint$(), {
    initialValue: null,
  });

  constructor() {
    void this.resolveInitialDestination();
  }

  readonly showBootstrapBanner = computed(() => {
    const url = this.currentUrl();
    const bootstrapModeActive =
      this.localSeedModeHint() ?? this.sessionBootstrapState.sessionBootstrapState().seedModeActive;

    // Depending on runtime/base-href the URL may be '/dashboard' or '/graphql/dashboard'.
    return bootstrapModeActive && !url.includes('/dashboard');
  });

  readonly showAuthenticatedShell = computed(() => this.isAuthenticatedRoute(this.currentUrl()));

  private async resolveInitialDestination(): Promise<void> {
    await firstValueFrom(
      this.bootstrapService.bootstrapStatus('StartupProbe').pipe(
        switchMap(({ systemState }) => {
          if (systemState.seedModeActive) {
            this.sessionBootstrapState.markInitialRouteResolved();
            return this.navigateIfNeeded('/bootstrap');
          }

          const isAuthenticated = this.auth.isLoggedIn();
          this.sessionBootstrapState.markSessionKnown(isAuthenticated);
          this.sessionBootstrapState.markInitialRouteResolved();

          const targetUrl = this.resolveAuthenticatedStartupDestination(isAuthenticated);
          return targetUrl ? this.navigateIfNeeded(targetUrl) : of(true);
        }),
        catchError(() => {
          this.sessionBootstrapState.markSessionKnown(false);
          this.sessionBootstrapState.markInitialRouteResolved();
          return this.navigateIfNeeded('/login');
        }),
        take(1),
      ),
    );
  }

  private resolveAuthenticatedStartupDestination(isAuthenticated: boolean): string | null {
    const url = this.router.url || '/';
    if (!this.isStartupManagedUrl(url)) {
      return null;
    }

    if (isAuthenticated && this.isProtectedStartupUrl(url)) {
      return url;
    }

    return isAuthenticated ? '/dashboard' : '/login';
  }

  private isStartupManagedUrl(url: string): boolean {
    return (
      url === '/' ||
      url.length === 0 ||
      url.startsWith('/login') ||
      url.startsWith('/bootstrap') ||
      this.isProtectedStartupUrl(url)
    );
  }

  private isProtectedStartupUrl(url: string): boolean {
    return url.startsWith('/dashboard') || url.startsWith('/admin/') || url.startsWith('/playground');
  }

  private isAuthenticatedRoute(url: string): boolean {
    return url.includes('/dashboard') || url.includes('/admin/') || url.includes('/playground');
  }

  private navigateIfNeeded(targetUrl: string) {
    const currentUrl = this.router.url || '/';
    if (currentUrl === targetUrl) {
      return of(true);
    }

    return from(this.router.navigateByUrl(targetUrl, { replaceUrl: true }));
  }
}
