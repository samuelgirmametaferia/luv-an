# ADR-002: SAO Pointer Encoding and Metadata Fallback

- **Status**: Accepted
- **Decision**: Encode pointer state in high/tag bits when supported (TBI-compatible); otherwise use software metadata mapping.
- **Rationale**: Preserves fast-path compactness while guaranteeing hardware-independent semantics via fallback.
- **Consequences**: Capability probing is mandatory; unsupported hosts transparently switch to metadata mode.
