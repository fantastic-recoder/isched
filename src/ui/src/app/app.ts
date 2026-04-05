import { Component, computed, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NavigationEnd, Router, RouterOutlet } from '@angular/router';
import { toSignal } from '@angular/core/rxjs-interop';
import { catchError, filter, map, merge, of, startWith, switchMap } from 'rxjs';
import { BootstrapService } from './services/bootstrap.service';

@Component({
  selector: 'app-root',
  imports: [CommonModule, RouterOutlet],
  templateUrl: './app.html',
  styleUrl: './app.scss'
})
export class App {
  private readonly bootstrapService = inject(BootstrapService);
  private readonly router = inject(Router);

  private readonly backendBootstrapMode = toSignal(
    merge(
      // Initial check at app startup.
      of(null),
      // Refresh bootstrap status whenever navigation completes.
      this.router.events.pipe(filter((event): event is NavigationEnd => event instanceof NavigationEnd)),
    ).pipe(
      switchMap(() => this.bootstrapService.bootstrapStatus()),
      map(({ systemState }) => systemState.seedModeActive),
      catchError(() => of(false)),
    ),
    { initialValue: false },
  );

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

  private readonly isBootstrapMode = computed(
    () => this.localSeedModeHint() ?? this.backendBootstrapMode(),
  );

  readonly showBootstrapBanner = computed(() => {
    const url = this.currentUrl();
    // Depending on runtime/base-href the URL may be '/dashboard' or '/graphql/dashboard'.
    return this.isBootstrapMode() && !url.includes('/dashboard');
  });
}
