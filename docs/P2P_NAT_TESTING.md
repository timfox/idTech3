# P2P NAT testing

## CI tiers

| Tier | Script | When it runs |
|------|--------|----------------|
| Unit | `unit_p2p_stun` | Every `ctest` via `linux-full` |
| Simulation | `test_p2p_nat_sim.sh` | Every `ctest` (STUN probe + optional coturn) |
| Guard | `test_p2p_ice_guard.sh` | Every `ctest` static; optional live spoof harness |
| Reconnect | `test_p2p_reconnect.sh` | Static + auto live against `rtest_base` when server/pack exist |
| Migrate | `test_p2p_migrate.sh` | Static wiring for backup-host + `p2pMigrate` |
| Live two-runner | `test_p2p_nat_live.sh` | Gated workflow `.github/workflows/p2p-nat.yml` |

## Simulation (`test_p2p_nat_sim`)

```bash
cd build-vk-Release && ctest -R test_p2p_nat_sim --output-on-failure
```

Skip flags:

- `SKIP_P2P_NAT_SIM=1` — skip entire script
- `SKIP_P2P_COTURN=1` — skip Docker coturn allocate probe

## ICE guard (`test_p2p_ice_guard`)

```bash
cd build-vk-Release && ctest -R test_p2p_ice_guard --output-on-failure
```

Optional live spoof harness:

- `IDTECH3_P2P_ICE_GUARD_LIVE=1` — launch a dedicated server, begin a direct-UDP ICE connect path, inject spoofed `p2pCand` / `p2pCheckAck`, and verify the active peer guard rejects them
- `P2P_ICE_GUARD_GAME_BASE=/abs/path/to/base-parent` — optional runnable data tree for the live harness; when unset, the harness skips cleanly if the dedicated server exits with `No game data`

## Reconnect (`test_p2p_reconnect`)

```bash
cd build-vk-Release && ctest -R test_p2p_reconnect --output-on-failure
```

Static wiring always runs. Live OOB reconnect auto-runs when `idtech3_server` and `docs/renderer_validation/devdata/rtest_base` are present: `p2p_grace_prime` seeds a grace slot (Huffman `connect` is not required), then `p2pReconnect` must return `challengeResponse`. Skip live with `SKIP_P2P_RECONNECT_LIVE=1`; force-fail if missing with `IDTECH3_P2P_RECONNECT_LIVE=1`. Override pack via `P2P_RECONNECT_GAME_BASE` / `P2P_RECONNECT_GAME_DIR`.

## Migrate (`test_p2p_migrate`)

```bash
cd build-vk-Release && ctest -R test_p2p_migrate --output-on-failure
```

Static checks for `cl_p2pBackupHost`, `p2pMigrate` OOB, and advertised `p2pmigrate`. Skip with `SKIP_P2P_MIGRATE=1`.

## Live two-runner job

Requires repository variable or secret `IDTECH3_P2P_NAT_TEST` (any non-empty value).

Self-hosted runners:

- `idtech3-p2p-host` — runs host role, uploads `p2p-nat-host.json` artifact
- `idtech3-p2p-client` — downloads artifact, punches host, asserts `ack:yes`

Optional variable `P2P_NAT_HOST_IP` sets the advertised host IP in the host artifact (default `127.0.0.1` for same-machine smoke).

Local dry-run:

```bash
export IDTECH3_P2P_NAT_LIVE=1
./tests/scripts/test_p2p_nat_live.sh host
./tests/scripts/test_p2p_nat_live.sh client
```

## Runner labels

Tag machines with GitHub Actions labels:

- `self-hosted`
- `idtech3-p2p-host` or `idtech3-p2p-client`

For real NAT fidelity, host and client runners should sit behind different NATs and set `P2P_NAT_HOST_IP` to the host's reachable public or LAN address.
