import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
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
    return this.gql.query<{ roles: RoleRecord[] }>('query { roles { id name scope } }');
  }
}

