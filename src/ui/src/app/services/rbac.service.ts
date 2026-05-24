import { Injectable, inject } from '@angular/core';
import { map, Observable } from 'rxjs';
import { GraphQLService } from './graphql.service';

export interface RoleRecord {
  id: string;
  name: string;
  scope: string;
}

@Injectable({ providedIn: 'root' })
export class RbacService {
  private readonly gql = inject(GraphQLService);

  listRoles(): Observable<{ roles: RoleRecord[] }> {
    return this.gql
      .query<{ isched: { roles: RoleRecord[] } }>('query { isched { roles { id name scope } } }')
      .pipe(map(({ isched }) => ({ roles: isched.roles })));
  }
}

