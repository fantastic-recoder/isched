import { ApplicationConfig, APP_INITIALIZER, provideBrowserGlobalErrorListeners, inject } from '@angular/core';
import { provideRouter, withNavigationErrorHandler } from '@angular/router';
import { provideHttpClient, withInterceptors } from '@angular/common/http';
import { NavigationError } from '@angular/router';
import { catchError, firstValueFrom, of } from 'rxjs';

import { routes } from './app.routes';
import { authInterceptor } from './interceptors/auth.interceptor';
import { AuthService } from './services/auth.service';

function initializeAuthSession(): () => Promise<void> {
  return () => {
    const authService = inject(AuthService);
    return firstValueFrom(
      authService.bootstrapSession().pipe(
        catchError(() => of(false)),
      ),
    ).then(() => undefined);
  };
}

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    provideRouter(
      routes,
      withNavigationErrorHandler((err: NavigationError) => {
        // UNAUTHENTICATED navigation errors are handled per-guard;
        // all others fall back to the default redirect in routes.
        console.warn('Navigation error', err);
      }),
    ),
    provideHttpClient(withInterceptors([authInterceptor])),
    {
      provide: APP_INITIALIZER,
      multi: true,
      useFactory: initializeAuthSession,
    },
  ],
};
