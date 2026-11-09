import { ChangeDetectionStrategy, Component, DestroyRef, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { GraphQLSubscriptionService } from '../../services/graphql-subscription.service';

const WS_PROBE = 'subscription { isched { healthChanged { status } } }';

@Component({
  selector: 'app-dev-proxy-health-page',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './dev-proxy-health.page.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DevProxyHealthPage {
  private readonly subscriptions = inject(GraphQLSubscriptionService);
  private readonly destroyRef = inject(DestroyRef);

  /** connecting | open | closed | failed — signal-backed. */
  readonly connection = this.subscriptions.connection;

  /** Opens a probe subscription over the proxied WebSocket; HTTP flows are unaffected. */
  probeWebSocket(): void {
    this.subscriptions
      .subscribe(WS_PROBE)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({ error: () => undefined });
  }
}
