# ADR-004: RC_SHARED via ALR/TLLB/PSF

- **Status**: Accepted
- **Decision**: Implement shared ownership with thread-local lease buffers (TLLB), asynchronous lease reclamation (ALR), and pressure-sensitive flushing (PSF).
- **Rationale**: Removes atomics from hot paths while preserving correctness through bounded asynchronous reconciliation.
- **Consequences**: Requires flush thresholds, reclamation fairness controls, and deterministic fallback for pressure/failure scenarios.
