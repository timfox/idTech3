# MSAA Transparency Policy

| Path | Policy |
|------|--------|
| Forward+ opaque | Native MSAA allowed |
| Deferred | Explicit single-sample G-buffer consumers |
| WBOIT | Single-sample production; MSAA×OIT not certified |
| Post | Resolved single-sample HDR |

Commands: `msaa_policy_status` · `msaa_policy_validate`.
