import { ChangeDetectionStrategy, Component } from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-dev-proxy-health-page',
  standalone: true,
  imports: [CommonModule],
  template: `
    <section class="p-6 space-y-3">
      <h1 class="text-2xl font-semibold">Dev Proxy Health</h1>
      <p>Use this page to validate same-origin /graphql proxy routing in local development.</p>
      <ul class="list-disc pl-5">
        <li>Ensure HTTP requests go to <code>/graphql</code>.</li>
        <li>Ensure WebSocket upgrade is proxied on <code>/graphql</code>.</li>
        <li>Ensure CSRF header is attached on mutations.</li>
      </ul>
    </section>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DevProxyHealthPage {}

