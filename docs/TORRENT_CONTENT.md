# Peer-Assisted Content Distribution

Optional libtorrent-powered package delivery lets the engine fetch signed `.pk3`
content through a controlled peer-assisted path for patches, optional packages,
and large community content. This is not a general BitTorrent client: the engine
only treats it as an alternate content transport behind the normal download
policy and package validation rules.

## Build

The feature is off by default:

```sh
cmake -S . -B build-vk-Release -DUSE_LIBTORRENT=ON
```

When enabled, CMake looks for `LibtorrentRasterbar::torrent-rasterbar`,
`libtorrent-rasterbar::torrent-rasterbar`, then the `libtorrent-rasterbar`
pkg-config package. If libtorrent is not found, the engine still builds the
control and validation path, but the transfer backend reports as unavailable.

License note: libtorrent-rasterbar is primarily BSD-3-Clause and compatible with
GPL-2.0 projects. Before shipping macOS builds, audit the Apple-specific
`include/libtorrent/aux_/route.h` path and any platform code pulled into the
binary.

## Runtime Controls

The torrent backend is opt-in at runtime:

```cfg
seta torrent_enable 0
seta torrent_port 0
seta torrent_downloadRate 0
seta torrent_uploadRate 0
seta torrent_maxConnections 80
seta torrent_maxPackageSize 8589934592
seta torrent_seedCompleted 0
seta torrent_allowPublicTrackers 0
seta torrent_allowDht 0
seta torrent_allowPex 0
seta torrent_allowLsd 1
seta torrent_requireSignature 1
seta torrent_debug 0
```

Official package distribution should keep public trackers, DHT, PEX, and
post-install seeding disabled unless a user or server operator explicitly opts
in. LAN discovery is enabled by default because it can help local installs
without publishing package participation globally.

Useful commands:

```cfg
torrent_status
torrent_checkmanifest manifests/base_patch.json
```

## Security Contract

Downloaded package manifests are validated before install work begins:

- Payload paths must be relative and cannot contain absolute paths, Windows
  drive prefixes, empty components, `.` components, or `..` traversal.
- Payloads are limited to `.pk3` files; native binaries and scripts are not
  accepted as torrent-installed files.
- Package size, file count, SHA-256 file hashes, and torrent info hashes are
  bounded and validated.
- Signatures are required by default with `torrent_requireSignature 1`.
- Transfer output should land in a quarantine directory first, then move into
  the game directory only after manifest, hash, and signature checks pass.

The current implementation provides the build gate, cvars, package URL routing,
manifest validation, and status/validation commands. The live session pump and
quarantine installer should be added on top of this boundary rather than
bypassing it.
