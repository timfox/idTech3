#!/usr/bin/env bash
# P2P host-migration: static wiring for backup-host promote + p2pMigrate OOB.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ "${SKIP_P2P_MIGRATE:-0}" = "1" ]; then
	pass "test_p2p_migrate skipped (SKIP_P2P_MIGRATE=1)"
	exit 0
fi

test -f runtime/client/core/cl_p2p_session.c || fail "missing cl_p2p_session.c"
grep -q 'CL_P2P_SessionBroadcastMigrate' runtime/client/core/cl_p2p_session.c || fail "missing migrate broadcast"
grep -q 'CL_P2P_SessionTryPromoteBackupHost' runtime/client/core/cl_p2p_session.c || fail "missing backup-host promote"
grep -q 'CL_P2P_SessionIsBackupHostEligible' runtime/client/core/cl_p2p_session.c || fail "missing backup-host eligibility"
grep -q 'cl_p2pBackupHost' runtime/client/core/cl_p2p_session.c || fail "missing cl_p2pBackupHost cvar"
grep -q 'p2pMigrate' runtime/client/core/cl_p2p_session.c || fail "missing p2pMigrate handler"
grep -q 'Q_stricmp( cmd, "p2pMigrate" )' runtime/client/core/cl_p2p_session.c || fail "missing p2pMigrate OOB match"
grep -q 'p2pMigrate' engine/core/net_p2p.c || fail "missing p2pMigrate allowlist in NET_P2P_HandleOobPacket"
grep -q 'p2pmigrate' runtime/server/core/sv_main.c || fail "missing advertised p2pmigrate info key"
grep -q 'sv_p2pHostMigration' runtime/server/core/sv_init.c || fail "missing sv_p2pHostMigration cvar"
grep -q 'p2pfail=migrate' docs/P2P_NETWORKING.md || fail "missing migrate failover docs"
grep -q 'cl_p2pBackupHost' docs/P2P_NETWORKING.md || fail "missing backup-host docs"
grep -q 'Listen-host migration' docs/P2P_NETWORKING.md || fail "missing listen-host migration docs"

pass "test_p2p_migrate: static checks ok"
