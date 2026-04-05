import { ChangeDetectionStrategy, Component } from '@angular/core';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-dev-proxy-health-page',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './dev-proxy-health.page.html',
  styleUrl: './dev-proxy-health.page.scss',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DevProxyHealthPage {}

