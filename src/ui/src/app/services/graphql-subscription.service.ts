// SPDX-License-Identifier: MPL-2.0
/**
 * @file graphql-subscription.service.ts
 * @brief Lightweight GraphQL subscription client using the graphql-transport-ws
 *        protocol over a native browser WebSocket (SP-012).
 *
 * Design notes:
 * - Uses the standard sub-protocol `graphql-transport-ws`.
 * - One shared connection per service instance (singleton via `providedIn: root`).
 * - Reconnects with exponential back-off after unexpected close/error.
 * - Returns cold Observables; the connection is established lazily on the first
 *   subscriber and torn down when the last subscriber unsubscribes.
 * - Authentication is provided only by the browser-managed HTTP-only session
 *   cookie during the WebSocket handshake. Protocol payloads contain no token.
 * - Close code 4401 (unauthorized) is terminal: no reconnect, state becomes `failed`.
 */

import { Injectable, OnDestroy, signal } from '@angular/core';
import { Observable, Subject, Subscriber } from 'rxjs';

export type SubscriptionConnection = 'connecting' | 'open' | 'closed' | 'failed';

interface GqlTransportMessage {
  id?: string;
  type: string;
  payload?: unknown;
}

interface SubscriptionRecord {
  subscriber: Subscriber<unknown>;
  query: string;
  variables?: Record<string, unknown>;
}

const WS_GRAPHQL_PROTOCOL = 'graphql-transport-ws';
const INITIAL_RECONNECT_DELAY_MS = 500;
const MAX_RECONNECT_DELAY_MS = 30_000;
const KEEP_ALIVE_TIMEOUT_MS = 30_000;
export const WS_CLOSE_UNAUTHORIZED = 4401;

@Injectable({ providedIn: 'root' })
export class GraphQLSubscriptionService implements OnDestroy {
  private ws: WebSocket | null = null;
  private isConnected = false;
  private pendingSubscriptions = new Map<string, SubscriptionRecord>();
  private activeSubscriptions = new Map<string, SubscriptionRecord>();
  private nextId = 0;
  private reconnectDelay = INITIAL_RECONNECT_DELAY_MS;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private keepAliveTimer: ReturnType<typeof setTimeout> | null = null;
  private destroyed = false;
  private readonly connectionSignal = signal<SubscriptionConnection>('closed');

  /** Signal-backed connection state for diagnostics UI. */
  readonly connection = this.connectionSignal.asReadonly();

  /** Emits when the WebSocket connection is established or restored. */
  readonly connected$ = new Subject<void>();
  /** Emits when the WebSocket connection is lost. */
  readonly disconnected$ = new Subject<void>();

  /**
   * Subscribe to a GraphQL subscription operation.
   *
   * @param query       GraphQL subscription document string (must start with
   *                    `subscription`).
   * @param variables   Optional variables object.
   * @returns           Observable that emits each `next` payload from the
   *                    server.  Completes when the server sends `complete`.
   *                    Errors if the server sends an `error` message.
   */
  subscribe<T = unknown>(query: string, variables?: Record<string, unknown>): Observable<T> {
    return new Observable<T>((subscriber) => {
      const id = `sub-${++this.nextId}`;
      const record: SubscriptionRecord = { subscriber: subscriber as Subscriber<unknown>, query, variables };
      this.pendingSubscriptions.set(id, record);

      this.ensureConnected();

      if (this.isConnected) {
        this.sendSubscribe(id, record);
      }
      // else: will be flushed in onOpen()

      return () => {
        this.cancelSubscription(id);
      };
    });
  }

  ngOnDestroy(): void {
    this.destroyed = true;
    this.clearTimers();
    if (this.ws) {
      this.ws.close(1000, 'service destroyed');
      this.ws = null;
    }
  }

  // ── Private helpers ───────────────────────────────────────────────────────

  private ensureConnected(): void {
    if (this.destroyed) return;
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const url = `${protocol}//${window.location.host}/graphql`;

    this.connectionSignal.set('connecting');
    let ws: WebSocket;
    try {
      ws = new WebSocket(url, [WS_GRAPHQL_PROTOCOL]);
    } catch {
      this.connectionSignal.set('failed');
      this.scheduleReconnect();
      return;
    }
    this.ws = ws;

    ws.onopen = () => {
      // The browser attaches the HttpOnly session cookie to the handshake.
      // Never send authentication material in graphql-transport-ws messages.
      this.ws?.send(JSON.stringify({ type: 'connection_init' }));
    };

    ws.onmessage = (event: MessageEvent<string>) => {
      let msg: GqlTransportMessage;
      try {
        msg = JSON.parse(event.data) as GqlTransportMessage;
      } catch {
        return;
      }
      this.handleMessage(msg);
    };

    ws.onerror = () => {
      // onclose will follow; handled there.
    };

    ws.onclose = (event: CloseEvent) => {
      const wasOpen = this.isConnected;
      this.isConnected = false;
      this.ws = null;
      this.disconnected$.next();
      this.clearKeepAlive();
      if (event?.code === WS_CLOSE_UNAUTHORIZED) {
        // Terminal: credentials rejected. Surface to pending subscribers; no reconnect loop.
        this.connectionSignal.set('failed');
        this.failAll([{ message: 'Unauthorized', extensions: { code: 'UNAUTHENTICATED' } }]);
        return;
      }
      this.connectionSignal.set(wasOpen || this.destroyed ? 'closed' : 'failed');
      if (!this.destroyed) {
        this.scheduleReconnect();
      }
    };
  }

  private handleMessage(msg: GqlTransportMessage): void {
    switch (msg.type) {
      case 'connection_ack': {
        this.isConnected = true;
        this.connectionSignal.set('open');
        this.reconnectDelay = INITIAL_RECONNECT_DELAY_MS;
        this.connected$.next();
        this.resetKeepAlive();
        // Flush all pending subscriptions
        for (const [id, record] of this.pendingSubscriptions) {
          this.sendSubscribe(id, record);
        }
        break;
      }
      case 'ping': {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
          this.ws.send(JSON.stringify({ type: 'pong' }));
        }
        this.resetKeepAlive();
        break;
      }
      case 'pong':
        this.resetKeepAlive();
        break;
      case 'next': {
        if (!msg.id) return;
        const record = this.activeSubscriptions.get(msg.id);
        if (record) {
          const payload = (msg.payload as { data?: unknown })?.data;
          record.subscriber.next(payload);
        }
        break;
      }
      case 'error': {
        if (!msg.id) return;
        const record = this.activeSubscriptions.get(msg.id);
        if (record) {
          const errors = Array.isArray(msg.payload) ? msg.payload : [msg.payload];
          record.subscriber.error(errors);
          this.activeSubscriptions.delete(msg.id);
        }
        break;
      }
      case 'complete': {
        if (!msg.id) return;
        const record = this.activeSubscriptions.get(msg.id);
        if (record) {
          record.subscriber.complete();
          this.activeSubscriptions.delete(msg.id);
        }
        break;
      }
      case 'connection_terminate':
        this.ws?.close(1000, 'server requested terminate');
        break;
    }
  }

  private failAll(errors: unknown[]): void {
    const all = [...this.pendingSubscriptions.values(), ...this.activeSubscriptions.values()];
    this.pendingSubscriptions.clear();
    this.activeSubscriptions.clear();
    for (const record of all) record.subscriber.error(errors);
  }

  private sendSubscribe(id: string, record: SubscriptionRecord): void {
    this.pendingSubscriptions.delete(id);
    this.activeSubscriptions.set(id, record);
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(
        JSON.stringify({
          id,
          type: 'subscribe',
          payload: { query: record.query, variables: record.variables ?? {} },
        }),
      );
    }
  }

  private cancelSubscription(id: string): void {
    this.pendingSubscriptions.delete(id);
    if (this.activeSubscriptions.has(id)) {
      this.activeSubscriptions.delete(id);
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify({ id, type: 'complete' }));
      }
    }
  }

  private scheduleReconnect(): void {
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      // Re-register all active subscriptions as pending so they are re-sent after reconnect.
      for (const [id, record] of this.activeSubscriptions) {
        this.pendingSubscriptions.set(id, record);
      }
      this.activeSubscriptions.clear();
      this.ensureConnected();
    }, this.reconnectDelay);

    this.reconnectDelay = Math.min(this.reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);
  }

  private resetKeepAlive(): void {
    this.clearKeepAlive();
    this.keepAliveTimer = setTimeout(() => {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify({ type: 'ping' }));
      }
    }, KEEP_ALIVE_TIMEOUT_MS);
  }

  private clearKeepAlive(): void {
    if (this.keepAliveTimer !== null) {
      clearTimeout(this.keepAliveTimer);
      this.keepAliveTimer = null;
    }
  }

  private clearTimers(): void {
    this.clearKeepAlive();
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }
}
