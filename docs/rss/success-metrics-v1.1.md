# RSS v1.1 Success Metrics + Exit Thresholds

These thresholds are release gates for RSS v1.1.

## 1. Correctness

- Differential suite requirement: **0 semantic divergences** between specialized and safe execution paths.
- Degradation equivalence requirement: **100% pass rate** for Tier 0/1/2/3 equivalence tests.

## 2. Throughput

- Tier 0 geometric mean throughput: **>= 1.30x** baseline runtime-safety build on release benchmark suite.

## 3. Latency

- Tier 1/2/3 degraded modes:
  - p95 latency regression **<= 15%** vs baseline safe build.
  - p99 latency regression **<= 25%** vs baseline safe build.

## 4. Memory

- Slab/allocator overhead at steady state: **<= 20%** above baseline safe build.
- Peak RSS growth in stress suite: **<= 30%** above baseline safe build.
- No unbounded growth in 24h soak test.

## 5. Degradation Reliability

- All forced-failure scenarios complete successfully with deterministic fallback behavior.
- Tier transition tests must show **0 undefined states** and **0 recovery deadlocks**.

## 6. Release Scorecard (must pass)

The release scorecard is pass/fail and includes:

1. Correctness gate
2. Throughput gate
3. Latency gate
4. Memory gate
5. Degradation gate
6. ADR + non-goal compliance gate
