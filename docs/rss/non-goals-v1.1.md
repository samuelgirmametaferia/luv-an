# RSS v1.1 Non-Goals Contract

This document defines work explicitly deferred from RSS v1.1.

## Deferred Items

| Item | Why Deferred | Bucket |
|---|---|---|
| Adaptive multi-model predictor ensembles for PBS | v1.1 only needs single calibrated probabilistic model | v1.2 |
| Cross-device NUMA-aware lease orchestration | Requires additional scheduler/runtime substrate | research |
| Architecture-specific micro-tuning beyond baseline x86_64/aarch64 profiles | Not required for correctness or first release goals | v1.2 |
| Exotic page-size tuning beyond base+2MB huge page support | Adds complexity with limited near-term ROI | research |
| Full GUI dashboards for RSS observability | CLI reports are sufficient for v1.1 signoff | v1.2 |
| Automatic kernel-module integration for trap acceleration | Environment-dependent and not release-critical | blocked-by-hardware |

## Explicitly Deferred Optional Enhancements

- Advanced predictor variants and online ensemble selection.
- Exotic architecture tuning and vendor-specific fast paths.
- Non-critical tooling polish that does not change correctness/safety.

## Lock Policy

- At branch cut for RSS v1.1, this list is frozen.
- Any non-goal change requires:
  1. A corresponding ADR update or new ADR.
  2. A note in release scorecard change log.
  3. Approval from RSS maintainers.
