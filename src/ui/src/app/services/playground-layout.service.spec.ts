// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-layout.service.spec.ts
 * @brief Unit tests for PlaygroundLayoutService (SP-011)
 */

import { TestBed } from '@angular/core/testing';
import { PlaygroundLayoutService, DEFAULT_LAYOUT } from './playground-layout.service';

const STORAGE_KEY = 'isched.playground.layout';

describe('PlaygroundLayoutService', () => {
  beforeEach(() => {
    localStorage.clear();
    TestBed.configureTestingModule({ providers: [PlaygroundLayoutService] });
  });

  it('returns default layout when localStorage is empty', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    expect(service.layout()).toEqual(DEFAULT_LAYOUT);
  });

  it('restores persisted layout from localStorage', () => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ leftWidthPct: 40, topHeightPct: 60 }));
    const service = TestBed.inject(PlaygroundLayoutService);
    expect(service.layout().leftWidthPct).toBe(40);
    expect(service.layout().topHeightPct).toBe(60);
  });

  it('falls back to default when localStorage contains invalid JSON', () => {
    localStorage.setItem(STORAGE_KEY, '{ not valid json }}}');
    const service = TestBed.inject(PlaygroundLayoutService);
    expect(service.layout()).toEqual(DEFAULT_LAYOUT);
  });

  it('falls back to default when localStorage contains wrong shape', () => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ foo: 'bar' }));
    const service = TestBed.inject(PlaygroundLayoutService);
    expect(service.layout()).toEqual(DEFAULT_LAYOUT);
  });

  it('clamps leftWidthPct to minimum 20', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setLeftWidth(5);
    expect(service.layout().leftWidthPct).toBe(20);
  });

  it('clamps leftWidthPct to maximum 80', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setLeftWidth(95);
    expect(service.layout().leftWidthPct).toBe(80);
  });

  it('clamps topHeightPct to minimum 15', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setTopHeight(5);
    expect(service.layout().topHeightPct).toBe(15);
  });

  it('clamps topHeightPct to maximum 85', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setTopHeight(99);
    expect(service.layout().topHeightPct).toBe(85);
  });

  it('persists values to localStorage after setLeftWidth', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setLeftWidth(45);
    const stored = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? '{}');
    expect(stored.leftWidthPct).toBe(45);
  });

  it('persists values to localStorage after setTopHeight', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setTopHeight(70);
    const stored = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? '{}');
    expect(stored.topHeightPct).toBe(70);
  });

  it('accepts valid boundary values without clamping', () => {
    const service = TestBed.inject(PlaygroundLayoutService);
    service.setLeftWidth(20);
    expect(service.layout().leftWidthPct).toBe(20);
    service.setLeftWidth(80);
    expect(service.layout().leftWidthPct).toBe(80);
  });
});

