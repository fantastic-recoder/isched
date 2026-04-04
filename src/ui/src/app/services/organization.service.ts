import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { GraphQLService } from './graphql.service';

export interface Organization {
  id: string;
  name: string;
  status: string;
}

@Injectable({ providedIn: 'root' })
export class OrganizationService {
  private readonly gql = inject(GraphQLService);

  list(): Observable<{ organizations: Organization[] }> {
    return this.gql.query<{ organizations: Organization[] }>(
      'query { organizations { id name status } }',
    );
  }
}

