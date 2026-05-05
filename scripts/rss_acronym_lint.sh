#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CANONICAL='RSS|SAO|VAS|SMS|ALR|TLLB|PSF|DEI|DEEM|HIL'
LEGACY_PATTERNS=(
  'Runtime Semantic Subsystem'
  'Virtual Shadow Mapping'
  'Slab Shadow Memory'
  'Async RC Reclamation'
  'Dual Entry Mode'
)

TARGET_GLOBS=(
  "docs/rss/*.md"
  "docs/rss/adr/*.md"
  "todo_rss.md"
  "src/**/*.cpp"
  "include/**/*.h"
)

status=0

echo "[rss-lint] checking for legacy terms..."
for pat in "${LEGACY_PATTERNS[@]}"; do
  if rg -n --hidden --glob '!.git' --glob '.junie/**' --glob '.copilot/**' "$pat" . >/dev/null; then
    echo "[rss-lint] error: found legacy term: $pat"
    rg -n --hidden --glob '!.git' --glob '.junie/**' --glob '.copilot/**' "$pat" .
    status=1
  fi
done

echo "[rss-lint] checking acronym casing..."
if rg -n --hidden --glob '!.git' --glob '.junie/**' --glob '.copilot/**' '\b(rss|sao|vas|sms|alr|tllb|psf|dei|deem|hil)\b' . >/dev/null; then
  echo "[rss-lint] error: found lowercase acronym forms"
  rg -n --hidden --glob '!.git' --glob '.junie/**' --glob '.copilot/**' '\b(rss|sao|vas|sms|alr|tllb|psf|dei|deem|hil)\b' .
  status=1
fi

echo "[rss-lint] canonical acronym set: $CANONICAL"

if [[ "$status" -ne 0 ]]; then
  echo "[rss-lint] FAILED"
  exit 1
fi

echo "[rss-lint] OK"
