# ADR-003: VAS/SMS Mapping Design and Huge-Page Policy

- **Status**: Accepted
- **Decision**: Use dual RO/RW mappings (VAS) over 64MB slabs (SMS), preferring 2MB huge pages when available.
- **Rationale**: Shifts enforcement to MMU, controls VMA pressure, and improves TLB locality.
- **Consequences**: Runtime must support fallback to base pages and emit capability reasons when huge pages are unavailable.
