export type OperationDigestState = 'idle' | 'loading' | 'success' | 'error';

export interface NavigationItem {
  id: string;
  label: string;
  route: string;
  visible?: boolean;
}

export interface ShellNavigationItem extends NavigationItem {
  active: boolean;
  visible: boolean;
}

export interface OperationDigest {
  message: string;
  state: OperationDigestState;
  operationKey: string;
  updatedAt: string;
  sequence: number;
}

export interface SessionIdentitySummary {
  displayName: string;
  userId?: string;
  resolved: boolean;
  fallbackLabel: string;
}

export interface ShellViewModel {
  logoAssetPath: string;
  navigation: ShellNavigationItem[];
  operationDigest: OperationDigest;
  identity: SessionIdentitySummary;
  authenticatedShellVisible: boolean;
}

export const SHELL_FALLBACK_USER_LABEL = 'Signed-in user';
export const SHELL_LOGO_ASSET_PATH = 'assets/isched_logo.jpg';

export const PRIMARY_NAVIGATION: readonly NavigationItem[] = [
  { id: 'dashboard', label: 'Dashboard', route: '/dashboard', visible: true },
  { id: 'organizations', label: 'Organizations', route: '/admin/organizations', visible: true },
  { id: 'users', label: 'Users', route: '/admin/users', visible: true },
  { id: 'rbac', label: 'RBAC', route: '/admin/rbac', visible: true },
] as const;

export function createInitialOperationDigest(): OperationDigest {
  return {
    message: 'Ready',
    state: 'idle',
    operationKey: 'shell:idle',
    updatedAt: new Date().toISOString(),
    sequence: 0,
  };
}

export function createFallbackIdentity(): SessionIdentitySummary {
  return {
    displayName: SHELL_FALLBACK_USER_LABEL,
    resolved: false,
    fallbackLabel: SHELL_FALLBACK_USER_LABEL,
  };
}

