# ADR-005: DEI Dual-Entry Specialization

- **Status**: Accepted
- **Decision**: Emit `Entry_Speculative` and `Entry_Safe` for eligible functions and dispatch through a dynamic entry table.
- **Rationale**: Enables aggressive fast paths without sacrificing deterministic fallback semantics.
- **Consequences**: ABI consistency across entries is mandatory; deopt/switch paths must preserve full program semantics.
