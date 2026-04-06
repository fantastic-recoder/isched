import { Injectable, signal } from '@angular/core';
import {
  createFallbackIdentity,
  createInitialOperationDigest,
  OperationDigest,
  OperationDigestState,
  SessionIdentitySummary,
} from './shell-status.models';

interface DigestUpdateInput {
  message: string;
  state: OperationDigestState;
  operationKey: string;
  sequence?: number;
}

interface IdentityUpdateInput {
  displayName?: string | null;
  userId?: string | null;
}

@Injectable({ providedIn: 'root' })
export class ShellStatusService {
  private readonly operationDigestSignal = signal<OperationDigest>(createInitialOperationDigest());
  private readonly identitySignal = signal<SessionIdentitySummary>(createFallbackIdentity());
  private nextSequenceValue = 0;

  readonly operationDigest = this.operationDigestSignal.asReadonly();
  readonly identity = this.identitySignal.asReadonly();

  beginOperation(operationKey: string, message: string): number {
    return this.publishDigest({ operationKey, message, state: 'loading' });
  }

  completeOperation(operationKey: string, message: string, sequence: number): void {
    this.publishDigest({ operationKey, message, state: 'success', sequence });
  }

  failOperation(operationKey: string, message: string, sequence: number): void {
    this.publishDigest({ operationKey, message, state: 'error', sequence });
  }

  setIdentity({ displayName, userId }: IdentityUpdateInput): void {
    const normalizedDisplayName = displayName?.trim() ?? '';
    if (!normalizedDisplayName) {
      this.clearIdentity();
      return;
    }

    this.identitySignal.set({
      displayName: normalizedDisplayName,
      userId: userId?.trim() || undefined,
      resolved: true,
      fallbackLabel: createFallbackIdentity().fallbackLabel,
    });
  }

  clearIdentity(): void {
    this.identitySignal.set(createFallbackIdentity());
  }

  reset(): void {
    this.nextSequenceValue = 0;
    this.operationDigestSignal.set(createInitialOperationDigest());
    this.clearIdentity();
  }

  private publishDigest({ operationKey, message, state, sequence }: DigestUpdateInput): number {
    const currentDigest = this.operationDigestSignal();
    const nextSequence = sequence ?? this.generateSequence();

    this.nextSequenceValue = Math.max(this.nextSequenceValue, nextSequence);
    if (nextSequence < currentDigest.sequence) {
      return currentDigest.sequence;
    }

    this.operationDigestSignal.set({
      operationKey,
      state,
      message: this.normalizeMessage(message, currentDigest.message),
      updatedAt: new Date().toISOString(),
      sequence: nextSequence,
    });

    return nextSequence;
  }

  private generateSequence(): number {
    this.nextSequenceValue += 1;
    return this.nextSequenceValue;
  }

  private normalizeMessage(message: string, fallbackMessage: string): string {
    const normalizedMessage = message.trim();
    return normalizedMessage.length > 0 ? normalizedMessage : fallbackMessage;
  }
}

