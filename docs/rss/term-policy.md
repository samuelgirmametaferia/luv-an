# RSS Terminology Policy (No Synonym Drift)

Canonical terms are defined in `docs/rss/glossary.md`.

## Disallowed Legacy/Synonym Forms

- `Runtime Semantic Subsystem` (use `Runtime Semantic System`)
- `Virtual Shadow Mapping` (use `Virtual Alias Shadowing`)
- `Slab Shadow Memory` (use `Slab-Mapped Shadowing`)
- `Async RC Reclamation` (use `Asynchronous Lease Reclamation`)
- `Dual Entry Mode` (use `Dual-Entry Inlining`)

## Enforcement

- `scripts/rss_acronym_lint.sh` validates canonical acronym usage and blocks legacy forms.
- New term introductions must update `docs/rss/glossary.md` in the same change.
