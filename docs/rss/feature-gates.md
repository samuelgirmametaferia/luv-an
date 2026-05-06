# RSS v1.1 Feature Gate Matrix

## Compile-Time Flags

| Flag | Subsystem | Default | Sensitivity |
|---|---|---|---|
| `RSS_SAO` | Segmented Alias Offsetting | ON | correctness-sensitive |
| `RSS_VAS` | Virtual Alias Shadowing | ON | correctness-sensitive |
| `RSS_SMS` | Slab-Mapped Shadowing | ON | performance+correctness |
| `RSS_ALR` | ALR/TLLB/PSF shared leasing | ON | correctness-sensitive |
| `RSS_DEI` | Dual-entry specialization | ON | performance-sensitive |
| `RSS_HIL` | Hardware Independence Layer | ON | correctness-sensitive |

## Runtime Toggles

Runtime names mirror compile flags:

- `rss.sao`
- `rss.vas`
- `rss.sms`
- `rss.alr`
- `rss.dei`
- `rss.hil`

## Precedence Rules

1. CLI flags (highest)
2. Environment variables
3. Config file
4. Built-in defaults (lowest)

If compile-time flag is OFF, runtime cannot force it ON.

## Safe Defaults (Fail-Closed)

- If capability probing is uncertain for correctness-sensitive features, transition to safe tier (Tier 1+).
- Any unsupported critical feature is disabled with deterministic fallback enabled.
- Runtime never enables speculative behavior without a valid safe-entry path.

## Startup Capability Report Contract

Runtime must print/emit a structured startup report:

```text
[RSS] tier=Tier1 reason="VAS unsupported on host MMU mode"
[RSS] gate RSS_SAO=enabled source=default
[RSS] gate RSS_VAS=disabled source=capability_probe reason="dual-map not available"
[RSS] gate RSS_SMS=enabled source=default
[RSS] gate RSS_ALR=enabled source=config
[RSS] gate RSS_DEI=enabled source=default
[RSS] gate RSS_HIL=enabled source=default
```

Required fields per gate:

- gate name
- enabled/disabled
- source (`cli|env|config|default|capability_probe`)
- reason (mandatory when disabled)
