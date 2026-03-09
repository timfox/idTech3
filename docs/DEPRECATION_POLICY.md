# Deprecation Policy

This document defines how the project handles deprecation of APIs, cvars, and features.

## Principles

1. **Stability first**: Avoid breaking existing mods and configurations
2. **Clear communication**: Deprecations are announced in CHANGELOG
3. **Reasonable timeline**: At least one minor release between deprecation and removal
4. **Migration path**: Provide alternatives or migration steps when possible

## Deprecation Process

### 1. Announcement

- Add entry to CHANGELOG.md under `### Deprecated`
- Include: what is deprecated, why, recommended alternative, removal target version
- Example: `r_oldCvar (use r_newCvar instead; will be removed in v1.2.0)`

### 2. Warning Period

- **Cvars**: Log a one-time warning when the deprecated cvar is used: `"r_oldCvar is deprecated; use r_newCvar. Will be removed in v1.2.0"`
- **APIs**: Document in header comments and LUA_API.md
- **Minimum duration**: One minor release (e.g. deprecate in 1.1.0, remove in 1.2.0)

### 3. Removal

- Remove in the announced version
- Add CHANGELOG entry under `### Removed`
- Update all documentation

## Scope

### In Scope

- Public cvars (r_*, cl_*, com_*, etc.)
- Lua/script API functions
- File formats (if we ever change them)
- Network protocol (would be MAJOR version)

### Out of Scope

- Internal functions not exposed to mods
- Build system internals
- Third-party library APIs (we track upstream)

## Exceptions

- **Security**: Immediate removal if a cvar/API is a security risk
- **Critical bugs**: Fix may change behavior; document in CHANGELOG
- **Legal**: Remove if required by license or legal obligation

## References

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- CLAUDE.md (project constitution)
- CHANGELOG.md (version history)
