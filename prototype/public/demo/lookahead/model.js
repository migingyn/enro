// Illustrative demo assumptions, not calibrated probabilities or live sensor data.
// All times are minutes after midnight. Every run uses the same 2,048 draws so
// changing one assumption compares the same possible futures.
const NOW = 8 * 60 + 5;
const EVENT_AT = 9 * 60;
const SAMPLE_COUNT = 2048;

const duration = (label, min, mode, max) => Object.freeze({ label, min, mode, max });
const scenario = (id, label, context, startDelay, preparation) => Object.freeze({
  id, label, context,
  durations: Object.freeze({
    startDelay,
    preparation,
    travel: duration('Travel', 22, 26, 30),
    arrivalBuffer: duration('Walk to the destination', 2, 4, 6),
  }),
});

export const scenarios = Object.freeze({
  resting: scenario('resting', 'Still resting', 'Manually selected: lying down or occupied; preparation has not started.',
    duration('Time before getting started', 10, 18, 28),
    duration('Get ready', 14, 20, 26)),
  preparing: scenario('preparing', 'Getting ready', 'Manually selected: awake and actively preparing to leave.',
    duration('Remaining interruption', 0, 2, 6),
    duration('Finish getting ready', 12, 17, 22)),
  departing: scenario('departing', 'Leaving now', 'Manually selected: ready and heading out the door.',
    duration('Get out the door', 0, 0, 1),
    duration('Preparation complete', 0, 0, 0)),
});

function randomGenerator() {
  let seed = 0x65a201;
  return () => {
    seed = (seed + 0x6d2b79f5) | 0;
    let value = Math.imul(seed ^ (seed >>> 15), seed | 1);
    value ^= value + Math.imul(value ^ (value >>> 7), value | 61);
    return ((value ^ (value >>> 14)) >>> 0) / 4294967296;
  };
}

function triangular(u, { min, mode, max }) {
  if (max === min) return min;
  return u < (mode - min) / (max - min)
    ? min + Math.sqrt(u * (max - min) * (mode - min))
    : max - Math.sqrt((1 - u) * (max - min) * (max - mode));
}

function validateAdjustment(name, value, maximum) {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < 0 || value > maximum) {
    throw new RangeError(`${name} must be a finite number between 0 and ${maximum}.`);
  }
}

function quantile(sorted, fraction) {
  const index = (sorted.length - 1) * fraction;
  const lower = Math.floor(index);
  const upper = Math.ceil(index);
  return sorted[lower] + (sorted[upper] - sorted[lower]) * (index - lower);
}

/**
 * Run a scenario with independent triangular duration assumptions.
 * trafficMinutes adds to travel; delayMinutes adds before departure.
 * pLate means arrival after 09:00 under these assumptions; expectedLateMinutes
 * averages max(0, arrival - 09:00) over ALL samples, including on-time arrivals.
 * Histogram bins are fixed five-minute [from, to) intervals for comparisons.
 */
export function simulate({ stage = 'resting', trafficMinutes = 0, delayMinutes = 0 } = {}) {
  if (!Object.hasOwn(scenarios, stage)) throw new RangeError(`Unknown scenario: ${String(stage)}`);
  validateAdjustment('trafficMinutes', trafficMinutes, 20);
  validateAdjustment('delayMinutes', delayMinutes, 15);

  const selected = scenarios[stage];
  const random = randomGenerator();
  const arrivals = [];
  const bins = Array.from({ length: 23 }, (_, index) => ({ from: 500 + index * 5, to: 505 + index * 5, count: 0 }));
  let lateCount = 0;
  let totalLateMinutes = 0;

  for (let index = 0; index < SAMPLE_COUNT; index += 1) {
    let arrival = NOW + trafficMinutes + delayMinutes;
    for (const distribution of Object.values(selected.durations)) {
      arrival += triangular(random(), distribution);
    }
    arrivals.push(arrival);
    bins[Math.floor((arrival - 500) / 5)].count += 1;
    if (arrival > EVENT_AT) lateCount += 1;
    totalLateMinutes += Math.max(0, arrival - EVENT_AT);
  }

  arrivals.sort((left, right) => left - right);
  return {
    stage, now: NOW, eventAt: EVENT_AT, trafficMinutes, delayMinutes,
    pLate: lateCount / SAMPLE_COUNT,
    medianArrival: quantile(arrivals, 0.5),
    p10Arrival: quantile(arrivals, 0.1),
    p90Arrival: quantile(arrivals, 0.9),
    expectedLateMinutes: totalLateMinutes / SAMPLE_COUNT,
    bins, samples: SAMPLE_COUNT, scenario: selected,
  };
}
