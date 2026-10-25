import test from 'node:test';
import assert from 'node:assert/strict';
import { scenarios, simulate } from './model.js';

test('default scenarios land in the intended demo ranges with conserved samples', () => {
  const ranges = { resting: [552, 558], preparing: [533, 538], departing: [513, 519] };
  for (const [stage, [min, max]] of Object.entries(ranges)) {
    const result = simulate({ stage });
    assert.ok(result.medianArrival >= min && result.medianArrival <= max, `${stage}: ${result.medianArrival}`);
    assert.equal(result.samples, 2048);
    assert.equal(result.bins.reduce((sum, bin) => sum + bin.count, 0), result.samples);
    assert.ok(result.p10Arrival < result.medianArrival && result.medianArrival < result.p90Arrival);
    assert.ok(result.pLate >= 0 && result.pLate <= 1);
    assert.equal(result.now, 485);
    assert.equal(result.eventAt, 540);
    assert.equal(result.scenario, scenarios[stage]);
    for (const [index, bin] of result.bins.entries()) {
      assert.equal(bin.to - bin.from, 5);
      assert.ok(Number.isInteger(bin.count) && bin.count >= 0);
      if (index) assert.equal(result.bins[index - 1].to, bin.from);
    }
  }
});

test('sampling is deterministic and added time shifts all quantiles by that time', () => {
  for (const stage of Object.keys(scenarios)) {
    const baseline = simulate({ stage });
    assert.deepEqual(simulate({ stage }), baseline);
    const adjusted = simulate({ stage, trafficMinutes: 7, delayMinutes: 3 });
    for (const key of ['p10Arrival', 'medianArrival', 'p90Arrival']) {
      assert.ok(Math.abs(adjusted[key] - baseline[key] - 10) < 1e-9, `${stage} ${key}`);
    }
    // A ten-minute shift must move each histogram count exactly two bins.
    assert.deepEqual(adjusted.bins.map(bin => bin.count).slice(2), baseline.bins.map(bin => bin.count).slice(0, -2));
  }
});

test('traffic and delay cannot reduce lateness across their full supported ranges', () => {
  for (const stage of Object.keys(scenarios)) {
    for (const [name, maximum] of [['trafficMinutes', 20], ['delayMinutes', 15]]) {
      let previous = simulate({ stage });
      for (let minutes = 1; minutes <= maximum; minutes += 1) {
        const current = simulate({ stage, [name]: minutes });
        assert.ok(current.pLate >= previous.pLate);
        assert.ok(current.expectedLateMinutes >= previous.expectedLateMinutes);
        assert.ok(current.medianArrival > previous.medianArrival);
        assert.equal(current.bins.reduce((sum, bin) => sum + bin.count, 0), 2048);
        previous = current;
      }
    }
  }
});

test('advancing readiness reduces predicted lateness with shared assumptions', () => {
  for (const trafficMinutes of [0, 10, 20]) {
    for (const delayMinutes of [0, 7.5, 15]) {
      const results = ['resting', 'preparing', 'departing'].map(stage => simulate({ stage, trafficMinutes, delayMinutes }));
      for (let index = 1; index < results.length; index += 1) {
        assert.ok(results[index].medianArrival < results[index - 1].medianArrival);
        assert.ok(results[index].pLate <= results[index - 1].pLate);
        assert.ok(results[index].expectedLateMinutes <= results[index - 1].expectedLateMinutes);
        assert.equal(results[index].bins.reduce((sum, bin) => sum + bin.count, 0), 2048);
      }
    }
  }
  assert.ok(simulate({ stage: 'resting' }).pLate > simulate({ stage: 'preparing' }).pLate);
  assert.ok(simulate({ stage: 'preparing' }).pLate > simulate({ stage: 'departing' }).pLate);
});

test('rejects unknown stages and nonnumeric or out-of-range adjustments', () => {
  for (const stage of ['unknown', '__proto__', 'constructor', null, 3]) {
    assert.throws(() => simulate({ stage }), RangeError);
  }
  for (const [name, maximum] of [['trafficMinutes', 20], ['delayMinutes', 15]]) {
    for (const value of [-1, maximum + 0.01, NaN, Infinity, '5', null]) {
      assert.throws(() => simulate({ [name]: value }), RangeError);
    }
    assert.doesNotThrow(() => simulate({ [name]: maximum }));
  }
  assert.equal(Object.isFrozen(scenarios.resting.durations.travel), true);
});
