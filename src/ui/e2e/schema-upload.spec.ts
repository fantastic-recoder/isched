import { expect, test } from '@playwright/test';

const ADMIN_EMAIL = 'admin@e2e.test';
const ADMIN_PASSWORD = 'Str0ng!Password2025';
const ADMIN_DISPLAY_NAME = 'E2E Admin';
const SERVER_ORIGIN = `http://127.0.0.1:${process.env['ISCHED_SERVER_PORT'] ?? '18080'}`;

type GraphQlResponse<T> = {
  data?: T;
  errors?: Array<{
    message: string;
    extensions?: {
      code?: string;
    };
  }>;
};

type GraphQlOptions = {
  token?: string;
  csrfToken?: string;
};

async function gql<T>(
  request: import('@playwright/test').APIRequestContext,
  query: string,
  variables: Record<string, unknown> = {},
  options: GraphQlOptions = {},
): Promise<GraphQlResponse<T>> {
  const headers: Record<string, string> = {};
  headers['Origin'] = SERVER_ORIGIN;
  headers['Referer'] = `${SERVER_ORIGIN}/isched`;
  if (options.token) {
    headers['Authorization'] = `Bearer ${options.token}`;
  }
  if (options.csrfToken) {
    headers['X-CSRF-Token'] = options.csrfToken;
  }

  const response = await request.post('/graphql', {
    data: { query, variables },
    headers,
  });

  return (await response.json()) as GraphQlResponse<T>;
}

function ensureData<T>(result: GraphQlResponse<T>, label: string): T {
  if (result.errors && result.errors.length > 0) {
    throw new Error(`${label} failed: ${result.errors[0]?.message ?? 'unknown GraphQL error'}`);
  }
  if (!result.data) {
    throw new Error(`${label} failed: missing GraphQL data`);
  }
  return result.data;
}

async function ensurePlatformAdminToken(
  request: import('@playwright/test').APIRequestContext,
): Promise<string> {
  const loginResult = await gql<{ login: { token: string } }>(
    request,
    `mutation($email: String!, $password: String!) {
      login(email: $email, password: $password) { token }
    }`,
    { email: ADMIN_EMAIL, password: ADMIN_PASSWORD },
  );

  if (loginResult.data?.login?.token) {
    return loginResult.data.login.token;
  }

  const bootstrapResult = await gql<{ bootstrapPlatformAdmin: { token: string } }>(
    request,
    `mutation($input: BootstrapPlatformAdminInput!) {
      bootstrapPlatformAdmin(input: $input) { token }
    }`,
    {
      input: {
        email: ADMIN_EMAIL,
        password: ADMIN_PASSWORD,
        displayName: ADMIN_DISPLAY_NAME,
      },
    },
  );

  if (bootstrapResult.data?.bootstrapPlatformAdmin?.token) {
    return bootstrapResult.data.bootstrapPlatformAdmin.token;
  }

  const bootstrapErr = bootstrapResult.errors?.[0]?.message;
  const loginErr = loginResult.errors?.[0]?.message;
  throw new Error(`Unable to obtain platform-admin token. login='${loginErr}', bootstrap='${bootstrapErr}'`);
}

async function loginToken(
  request: import('@playwright/test').APIRequestContext,
  email: string,
  password: string,
  organizationId?: string,
): Promise<string> {
  const data = ensureData(
    await gql<{ login: { token: string } }>(
      request,
      `mutation($email: String!, $password: String!, $organizationId: ID) {
        login(email: $email, password: $password, organizationId: $organizationId) { token }
      }`,
      { email, password, organizationId: organizationId ?? null },
    ),
    `login(${email})`,
  );
  return data.login.token;
}

test.describe('Schema upload GraphQL flow', () => {
  test('uploads schemas and lists them with tenant scope isolation', async ({ request }) => {
    const unique = `${Date.now()}`;
    const adminToken = await ensurePlatformAdminToken(request);
    const adminCsrf = `csrf-admin-${unique}`;

    const createOrgMutation = `mutation($input: CreateOrganizationInput!) {
      createOrganization(input: $input) { id name }
    }`;

    const orgAData = ensureData(
      await gql<{ createOrganization: { id: string; name: string } }>(
        request,
        createOrgMutation,
        { input: { name: `Org A ${unique}` } },
        { token: adminToken, csrfToken: adminCsrf },
      ),
      'createOrganization(A)',
    );

    const orgBData = ensureData(
      await gql<{ createOrganization: { id: string; name: string } }>(
        request,
        createOrgMutation,
        { input: { name: `Org B ${unique}` } },
        { token: adminToken, csrfToken: adminCsrf },
      ),
      'createOrganization(B)',
    );

    const createUserMutation = `mutation($organizationId: ID!, $input: CreateUserInput!) {
      createUser(organizationId: $organizationId, input: $input) { id email }
    }`;

    const tenantAEmail = `tenant-a-${unique}@e2e.test`;
    const tenantBEmail = `tenant-b-${unique}@e2e.test`;
    const tenantPassword = 'TenantAdmin!Pass2026';

    ensureData(
      await gql<{ createUser: { id: string } }>(
        request,
        createUserMutation,
        {
          organizationId: orgAData.createOrganization.id,
          input: {
            email: tenantAEmail,
            password: tenantPassword,
            displayName: 'Tenant A Admin',
            roles: ['role_tenant_admin'],
          },
        },
        { token: adminToken, csrfToken: `csrf-admin-org-a-${unique}` },
      ),
      'createUser(tenant A admin)',
    );

    ensureData(
      await gql<{ createUser: { id: string } }>(
        request,
        createUserMutation,
        {
          organizationId: orgBData.createOrganization.id,
          input: {
            email: tenantBEmail,
            password: tenantPassword,
            displayName: 'Tenant B Admin',
            roles: ['role_tenant_admin'],
          },
        },
        { token: adminToken, csrfToken: `csrf-admin-org-b-${unique}` },
      ),
      'createUser(tenant B admin)',
    );

    const tenantAToken = await loginToken(
      request,
      tenantAEmail,
      tenantPassword,
      orgAData.createOrganization.id,
    );

    const tenantBToken = await loginToken(
      request,
      tenantBEmail,
      tenantPassword,
      orgBData.createOrganization.id,
    );

    const uploadMutation = `mutation($input: UploadSchemaDocumentInput!) {
      uploadSchemaDocument(input: $input) {
        success
        schema { name updatedBy }
        error { code message }
      }
    }`;

    const schemaAName = `schema-a-${unique}`;
    const schemaBName = `schema-b-${unique}`;
    const schemaAContent = 'type Query { tenantAField: String }';
    const schemaBContent = 'type Query { tenantBField: String }';

    const uploadA = ensureData(
      await gql<{ uploadSchemaDocument: { success: boolean; schema: { name: string } | null } }>(
        request,
        uploadMutation,
        { input: { name: schemaAName, content: schemaAContent, overwrite: false } },
        { token: tenantAToken, csrfToken: `csrf-tenant-a-${unique}` },
      ),
      'uploadSchemaDocument(tenant A)',
    );
    expect(uploadA.uploadSchemaDocument.success).toBe(true);
    expect(uploadA.uploadSchemaDocument.schema?.name).toBe(schemaAName);

    const uploadB = ensureData(
      await gql<{ uploadSchemaDocument: { success: boolean; schema: { name: string } | null } }>(
        request,
        uploadMutation,
        { input: { name: schemaBName, content: schemaBContent, overwrite: false } },
        { token: tenantBToken, csrfToken: `csrf-tenant-b-${unique}` },
      ),
      'uploadSchemaDocument(tenant B)',
    );
    expect(uploadB.uploadSchemaDocument.success).toBe(true);
    expect(uploadB.uploadSchemaDocument.schema?.name).toBe(schemaBName);

    const listQuery = `query {
      schemaDocuments {
        name
        createdAt
        updatedAt
        updatedBy
      }
    }`;

    const listA = ensureData(
      await gql<{ schemaDocuments: Array<{ name: string }> }>(
        request,
        listQuery,
        {},
        { token: tenantAToken },
      ),
      'schemaDocuments(tenant A)',
    );
    expect(listA.schemaDocuments.map((item) => item.name)).toContain(schemaAName);
    expect(listA.schemaDocuments.map((item) => item.name)).not.toContain(schemaBName);

    const listB = ensureData(
      await gql<{ schemaDocuments: Array<{ name: string }> }>(
        request,
        listQuery,
        {},
        { token: tenantBToken },
      ),
      'schemaDocuments(tenant B)',
    );
    expect(listB.schemaDocuments.map((item) => item.name)).toContain(schemaBName);
    expect(listB.schemaDocuments.map((item) => item.name)).not.toContain(schemaAName);
  });
});

