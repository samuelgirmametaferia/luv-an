# RSS v1.1 Frontend + Analysis Pipeline

This document locks Section 1 implementation details.

## IR Entry Points

Pipeline entry sequence is fixed:

1. **AST** (`Program`)
2. **CFG** (`rss::CFGModule`)
3. **MFG** (streamed `rss::MFGOp`)
4. **RSS analysis** (interprocedural pass manager)

Reference implementation:

- `include/rss/RSSPipeline.h`
- `src/rss/RSSPipeline.cpp`

## Incremental CFG Updates

- Full CFG build produces deterministic function ordering by name.
- Incremental update accepts `changedFunctions` set.
- For changed function:
  - rebuild function CFG
  - remove function if deleted
  - leave untouched functions unchanged

## Streaming MFG Builder

- MFG emission is sink/callback-based (`StreamSink`) to avoid mandatory full-graph materialization.
- Callers may collect streamed ops or process online.

## Interprocedural Pass Manager

Implemented pass manager supports:

- explicit dependency constraints (`dependsOn`)
- invalidation propagation (`invalidates`)
- deterministic pass stabilization with cycle/guard detection

Default pass chain:

1. `alias-propagation`
2. `probabilistic-specialization`
3. `hardware-mapping`
4. `emission-selection`

## SMIR + Emission Framework

SMIR is emitted after CFG/MFG analysis and before backend emission decisions.

### SMIR Schema Coverage

Each SMIR op (`rss::SMIROp`) captures:

- memory operation kind (`READ`, `WRITE`, `MUTATE`, `ALLOCATE`, `FREE`, `BORROW`, `MOVE`, `ESCAPE`, `CALL_BOUNDARY`)
- source location binding (`functionName`, `sourceNodeId`)
- state transition (`from`, `to`, `reason`)
- specialization metadata:
  - `pathLikelihood` in `[0,1]`
  - `assumptions[]`
  - `fallbackLink`
- fallback skeleton reference (`fallbackSkeletonRef`)

### Streaming Emission

- SMIR emission is streaming-compatible via `RSSPipeline::SMIRStreamSink`.
- Current pipeline derives SMIR from streamed MFG ops deterministically under fixed seed.

### Fallback Injection Contract

- Fallback skeleton references are injected at emission time.
- `specialization.fallbackLink` must match `fallbackSkeletonRef` for each op.

### Verification Contract

`SMIRVerifierReport` enforces:

1. well-formedness (ids, function/node bindings, known op kinds)
2. transition invariants (no invalid freed-to-live transitions)
3. specialization constraints (`pathLikelihood` bounds)
4. fallback completeness (skeleton ref and specialization link present + aligned)

Pipeline publishes `smir-verify`, `smir-op-count`, and `smir-issues` in pass outputs.

### Debug + Snapshot Outputs

- `RSSPipeline::dumpSMIR(...)` returns human-readable debug dumps.
- `RSSPipeline::snapshotSMIR(...)` returns a machine-readable JSON snapshot string.
- `AnalysisResult` stores both outputs (`smirDebugDump`, `smirSnapshot`).

## Profile Ingestion

- Probabilistic profile file format: `symbol=probability`
- Hardware profile file format:
  - `mmu=true|false`
  - `tlb_alias=true|false`
  - `tbi=true|false`
  - `huge_pages=true|false`
  - `trap_assist=true|false`

## Pass Ordering + Invalidation Rules

- A pass cannot run unless all dependencies have completed.
- If a pass invalidates another pass output, the invalidated pass is re-queued.
- Unresolvable dependencies fail fast with deterministic error.

## Determinism Contract

- Fixed seed drives stable node IDs and deterministic pass trace metadata.
- Function traversal order is lexicographically sorted.
- Under same AST + profiles + seed, pass outputs must match.
