# Security Policy

Thanks for helping keep this project safe. If you believe you’ve found a security vulnerability, please report it responsibly so we can fix it.

## Supported Versions

Security fixes are provided for:

- **Latest tagged release** (recommended)
- **`main`** (best-effort; may contain breaking changes)

Older tags/branches may not receive security updates. If you’re unsure whether a version is supported, report the issue anyway and include the commit/tag you tested.

> Tip: include the exact tag/commit hash and platform (Linux/Windows/macOS), since engine behavior can vary by build flags.

## Reporting a Vulnerability

### Preferred: GitHub Private Vulnerability Reporting
Please report vulnerabilities via **GitHub Security Advisories** (Private Vulnerability Reporting):
- Go to the repo → **Security** → **Report a vulnerability**

### What to include
To help reproduce and fix quickly, include:

- A clear description of the issue and why it’s security-relevant
- Affected version(s) / commit hash
- Platform + build configuration (compiler, CMake options, sanitizers, renderer, etc.)
- Repro steps or a minimal PoC (proof of concept)
- Any crash logs, backtraces, ASan/UBSan output, or screenshots
- If file-based: the smallest malicious asset/archive that triggers the issue (if safe to share)

### Response expectations
- **Acknowledgement:** within **72 hours**
- **Initial assessment / triage:** within **7 days**
- **Fix timeline:** depends on severity and complexity (critical issues are prioritized)

If the report is accepted, we’ll coordinate a disclosure timeline with you. If it’s declined, we’ll explain why.

## Scope

### In scope (examples)
- Remote code execution, sandbox escape, or privilege escalation
- Path traversal / arbitrary file write/read
- Unsafe parsing of network traffic (client/server protocol)
- Unsafe parsing of content archives/assets (e.g., `.pk3` or equivalent formats)
- Memory safety issues that appear exploitable (use-after-free, heap overflow, etc.)
- Supply-chain or update-related integrity issues (if applicable)

### Out of scope (examples)
- Non-exploitable crashes with no realistic security impact
- General gameplay cheating, griefing, or balance exploits
- Vulnerabilities in third-party dependencies that do not affect this project’s builds
- Issues requiring physical access only (unless they enable meaningful escalation)

If you’re not sure, report it anyway-better a false alarm than a silent RCE gremlin.

## Coordinated Disclosure

Please **do not publicly disclose** details (issues, PRs, social posts) until we’ve shipped a fix or agreed on a timeline.

We aim to follow coordinated disclosure norms (often ~90 days for publicly disclosed reports), but we’ll adjust based on severity and real-world risk.

## Safe Harbor / Testing Guidelines

- Do not test against systems you don’t own or lack permission to test.
- Do not disrupt public/community servers (no scanning, flooding, or DoS).
- Keep PoCs minimal and avoid payloads that cause harm beyond demonstrating the issue.

## Credit

If you want, we’re happy to credit reporters in release notes / advisories.
Include your preferred name/handle in the report.
