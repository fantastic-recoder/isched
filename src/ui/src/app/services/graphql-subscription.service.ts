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
 * - Token is NOT persisted — we rely on the `Authorization` cookie set by the
 *   auth flow (secure cookie-based flow); the WS upgrade carries the cookie
 *   automatically.
 */

import { Injectable, OnDestroy } from '@angular/core';
import { Observable, Subject, Subscriber } from 'rxjs';

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

    const ws = new WebSocket(url, [WS_GRAPHQL_PROTOCOL]);
    this.ws = ws;

    ws.onopen = () => {
      this.ws!.send(JSON.stringify({ type: 'connection_init', payload: {} }));
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

    ws.onclose = () => {
      this.isConnected = false;
      this.ws = null;
      this.disconnected$.next();
      this.clearKeepAlive();
      if (!this.destroyed) {
        this.scheduleReconnect();
      }
    };
  }

  private handleMessage(msg: GqlTransportMessage): void {
    switch (msg.type) {
      case 'connection_ack': {
        this.isConnected = true;
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

