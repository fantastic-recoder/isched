import { OperationLatencyRecorder } from './operation-latency.util';

describe('OperationLatencyRecorder', () => {
  it('records durations and computes nearest-rank percentiles', () => {
    let t = 0;
    const rec = new OperationLatencyRecorder(() => t);
    for (const d of [100, 200, 300, 400, 2000]) {
      const timer = rec.start('op');
      t += d;
      timer.stop();
    }
    expect(rec.all().length).toBe(5);
    expect(rec.percentile(50)).toBe(300);
    expect(rec.percentile(95)).toBe(2000);
    expect(Number.isNaN(rec.percentile(95, 'missing'))).toBe(true);
  });

  it('stop is idempotent', () => {
    let t = 0;
    const rec = new OperationLatencyRecorder(() => t);
    const timer = rec.start('x');
    t = 5;
    const a = timer.stop();
    t = 50;
    expect(timer.stop()).toBe(a);
    expect(rec.all().length).toBe(1);
  });
});
