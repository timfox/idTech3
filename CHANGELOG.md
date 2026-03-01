# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Water flowmap: flow vectors offset texture UVs for water surfaces (rivers, pools, wakes). Shader keywords `flowmapTex`, `flowSpeed`.
- Sample flowmap assets: `docs/samples/flowmap/` (shader, flowmap texture, README)
- Release checklist (`docs/RELEASE_CHECKLIST.md`)
- Release automation: CI attaches build artifacts to GitHub Releases on publish
- Quick start guide (`docs/QUICKSTART.md`) for end users
- CI shader compilation verification step
- `scripts/validate_ci_build.sh` for local CI validation
