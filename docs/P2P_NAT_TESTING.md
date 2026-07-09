# P2P NAT testing

## CI tiers

| Tier | Script | When it runs |
|------|--------|----------------|
| Unit | `unit_p2p_stun` | Every `ctest` via `linux-full` |
| Simulation | `test_p2p_nat_sim.sh` | Every `ctest` (STUN probe + optional coturn) |
| Live two-runner | `test_p2p_nat_live.sh` | Gated workflow `.github/workflows/p2p-nat.yml` |

## Simulation (`test_p2p_nat_sim`)

```bash
cd build-vk-Release && ctest -R test_p2p_nat_sim --output-on-failure
```

Skip flags:

- `SKIP_P2P_NAT_SIM=1` — skip entire script
- `SKIP_P2P_COTURN=1` — skip Docker coturn allocate probe

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
