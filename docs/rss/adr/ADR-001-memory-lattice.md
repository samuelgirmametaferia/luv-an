# ADR-001: Memory State Lattice and Monotonic Refinement

- **Status**: Accepted
- **Decision**: Use `UNKNOWN -> RC_SHARED -> OWNED_COW -> LENT -> UNIQUE` monotonic refinement with `ESCAPED` as region-scoped sink.
- **Rationale**: Ensures bounded convergence and deterministic join behavior across interprocedural analysis.
- **Consequences**: Invalid reverse transitions are disallowed; uncertainty escalates to safe fallback paths.
