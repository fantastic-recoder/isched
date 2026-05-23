import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component, computed, inject, signal } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { NavigationEnd, Router, RouterLink } from '@angular/router';
import { filter, finalize, map, startWith, take } from 'rxjs';
import { AuthService } from '../../services/auth.service';
import { ShellStatusService } from '../../services/shell-status.service';
import {
  PRIMARY_NAVIGATION,
  SHELL_LOGO_ASSET_PATH,
  ShellNavigationItem,
  ShellViewModel,
} from '../../services/shell-status.models';

@Component({
  selector: 'app-authenticated-shell',
  standalone: true,
  imports: [CommonModule, RouterLink],
  templateUrl: './authenticated-shell.component.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
  host: { class: 'block min-h-screen' },
})
export class AuthenticatedShellComponent {
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);
  private readonly shellStatus = inject(ShellStatusService);

  private readonly currentUrl = toSignal(
    this.router.events.pipe(
      filter((event): event is NavigationEnd => event instanceof NavigationEnd),
      map((event) => event.urlAfterRedirects),
      startWith(this.router.url),
    ),
    { initialValue: this.router.url },
  );

  readonly signingOut = signal(false);
  readonly operationDigest = this.shellStatus.operationDigest;
  readonly identity = this.shellStatus.identity;
  readonly navigationItems = computed<ShellNavigationItem[]>(() => {
    const currentUrl = this.normalizeUrl(this.currentUrl());

    return PRIMARY_NAVIGATION.filter((item) => item.visible !== false).map((item) => ({
      ...item,
      active: this.isRouteActive(item.route, currentUrl),
      visible: item.visible ?? true,
    }));
  });

  readonly currentUserLabel = computed(() => {
    const identity = this.identity();
    const displayName = identity.displayName.trim();
    return displayName.length > 0 ? displayName : identity.fallbackLabel;
  });

  readonly viewModel = computed<ShellViewModel>(() => ({
    logoAssetPath: SHELL_LOGO_ASSET_PATH,
    navigation: this.navigationItems(),
    operationDigest: this.operationDigest(),
    identity: this.identity(),
    authenticatedShellVisible: true,
  }));

  /** Routes where the content area should fill edge-to-edge (no padding). */
  readonly isFlushRoute = computed(() =>
    this.normalizeUrl(this.currentUrl()).startsWith('/playground')
  );

  signOut(): void {
    if (this.signingOut()) {
      return;
    }

    this.signingOut.set(true);
    this.auth
      .signOut()
      .pipe(
        take(1),
        finalize(() => this.signingOut.set(false)),
      )
      .subscribe({
        next: () => {
          void this.router.navigateByUrl('/login');
        },
        error: () => {
          void this.router.navigateByUrl('/login');
        },
      });
  }

  trackNavigationItem = (_index: number, item: ShellNavigationItem): string => item.id;

  private isRouteActive(route: string, currentUrl: string): boolean {
    return currentUrl === route || currentUrl.startsWith(`${route}/`);
  }

  private normalizeUrl(url: string): string {
    const [pathOnly] = url.split(/[?#]/, 1);
    return pathOnly || '/';
  }
}

