import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { GraphQLService } from './graphql.service';

export interface UserRecord {
  id: string;
  email: string;
  displayName: string;
  isActive: boolean;
}

@Injectable({ providedIn: 'root' })
export class UserService {
  private readonly gql = inject(GraphQLService);

  listUsers(organizationId: string): Observable<{ users: UserRecord[] }> {
    return this.gql.query<{ users: UserRecord[] }>('query { users { id email displayName isActive } }', {
      organizationId,
    });
  }
}

