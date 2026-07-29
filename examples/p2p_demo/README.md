# P2P Demo

Small standalone C++ UDP demo for proving the basic P2P message loop without
launching the full engine.

Build:

```bash
./examples/p2p_demo/build.sh
```

Run the proof:

```bash
./build/examples/p2p_demo/p2p_demo --self-test
```

Run interactively:

```bash
./build/examples/p2p_demo/p2p_demo
```

The menu has simple numbered buttons:

- `1` starts a local host socket.
- `2` connects the client to `127.0.0.1:<host-port>`.
- `3` sends `ping`.
- `4` runs the local proof test.

This is not a NAT traversal implementation. It is a tiny runnable companion for
the engine P2P stack: it demonstrates the host/client loop, UDP peer discovery
from inbound packets, and a visible `ping -> pong` proof.

