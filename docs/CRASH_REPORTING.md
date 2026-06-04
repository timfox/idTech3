# Crash reporting (opt-in)

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `com_crashReportEnable` | 0 | Write `crash_*.txt` on fatal errors |
| `com_crashReportURL` | "" | Optional HTTPS ingest (POST via `curl`, background) |

## Local report format

Plain text, no PII by default:

- engine id, version, map name, `fs_game`, timestamp, reason

## Server ingest (idtech3.com)

Document your privacy policy on the site. Suggested endpoint: `POST /api/v1/crash` with multipart file field `file`.

Implementation: [com_crash.c](../src/qcommon/com_crash.c).
