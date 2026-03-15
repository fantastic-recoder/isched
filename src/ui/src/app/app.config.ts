import { ApplicationConfig, provideBrowserGlobalErrorListeners } from '@angular/core';
import { provideRouter, withNavigationErrorHandler } from '@angular/router';
import { provideHttpClient, withInterceptors } from '@angular/common/http';
import { NavigationError } from '@angular/router';

import { routes } from './app.routes';
import { authInterceptor } from './interceptors/auth.interceptor';

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
  ],
};
