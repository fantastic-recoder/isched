import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { GraphQLService } from './graphql.service';

interface BootstrapStatusResponse {
  systemState: {
    seedModeActive: boolean;
  };
}

interface CompleteBootstrapResponse {
  bootstrapPlatformAdmin: {
    token: string;
    expiresAt: string;
  };
}

@Injectable({ providedIn: 'root' })
export class BootstrapService {
  private readonly gql = inject(GraphQLService);

  bootstrapStatus(): Observable<BootstrapStatusResponse> {
    return this.gql.query<BootstrapStatusResponse>('query { systemState { seedModeActive } }');
  }

  completeBootstrap(input: {
    email: string;
    password: string;
    displayName: string;
  }): Observable<CompleteBootstrapResponse> {
    return this.gql.mutate<CompleteBootstrapResponse>(
      `mutation($email: String!, $password: String!, $displayName: String) {
         bootstrapPlatformAdmin(input: { email: $email, password: $password, displayName: $displayName }) {
           token
           expiresAt
         }
       }`,
      input,
    );
  }
}

