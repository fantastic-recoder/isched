import { Component, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterOutlet } from '@angular/router';
import { catchError, map, of, shareReplay } from 'rxjs';
import { BootstrapService } from './services/bootstrap.service';

@Component({
  selector: 'app-root',
  imports: [CommonModule, RouterOutlet],
  templateUrl: './app.html',
  styleUrl: './app.scss'
})
export class App {
  private readonly bootstrapService = inject(BootstrapService);

  protected readonly isBootstrapMode$ = this.bootstrapService.bootstrapStatus().pipe(
    map(({ systemState }) => systemState.seedModeActive),
    catchError(() => of(false)),
    shareReplay({ bufferSize: 1, refCount: true }),
  );
}
