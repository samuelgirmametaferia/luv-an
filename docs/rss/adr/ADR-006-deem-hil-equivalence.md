# ADR-006: DEEM/HIL Degradation Equivalence Guarantees

- **Status**: Accepted
- **Decision**: Enforce DEEM (`Execution(D(S)) == Semantics(S)`) across Tier 0-3 using HIL capability detection and software emulation fallbacks.
- **Rationale**: Correctness cannot depend on hardware availability.
- **Consequences**: Every optimization path must have a total fallback path, and tier transitions must be deterministic.
