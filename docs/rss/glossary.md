# RSS v1.1 Canonical Glossary

This glossary is the single source of truth for RSS terminology.

- **RSS**: Runtime Semantic System; the execution substrate for hardware-symbiotic memory safety and specialization.
- **SAO**: Segmented Alias Offsetting; pointer/state encoding strategy using address/tag bits with software fallback.
- **VAS**: Virtual Alias Shadowing; dual RO/RW virtual mappings that shift safety checks to MMU protections.
- **SMS**: Slab-Mapped Shadowing; slab allocator and mapping model (64MB units) for scalable alias views.
- **ALR**: Asynchronous Lease Reclamation; batched reconciliation of shared-reference ownership updates.
- **TLLB**: Thread-Local Lease Buffer; per-thread lease staging area removing atomics from hot paths.
- **PSF**: Pressure-Sensitive Flushing; bounded partial lease flush under pressure/cold-path triggers.
- **DEI**: Dual-Entry Inlining; speculative and safe entrypoints per function with dynamic selection.
- **DEEM**: Degraded Execution Equivalence Model; guarantee that degraded execution preserves semantics.
- **HIL**: Hardware Independence Layer; capability detection plus software emulation for optional hardware features.

## Terminology Lock Rules

1. Use only glossary names in docs, comments, design notes, and PR text.
2. Deprecated/legacy synonyms are prohibited and should be replaced in the same change.
3. Any new RSS term requires glossary update in the same PR.
