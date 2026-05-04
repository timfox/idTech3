/**
 * Templated WebSocket server for idTech3 / browser demos.
 * Text messages are echoed. JSON { "type": "ping" } is answered with pong + server time.
 *
 * Usage: node echo_server.mjs
 * Env:   PORT (default 8765), HOST (default 0.0.0.0)
 */
import { WebSocketServer } from "ws";

const port = Number(process.env.PORT) || 8765;
const host = process.env.HOST || "0.0.0.0";

const wss = new WebSocketServer({ port, host });

wss.on("listening", () => {
  // eslint-disable-next-line no-console
  console.log(`[websocket-example] listening ws://${host}:${port}/`);
});

wss.on("connection", (ws, req) => {
  const from = req.socket?.remoteAddress ?? "unknown";
  // eslint-disable-next-line no-console
  console.log(`[websocket-example] client connected from ${from}`);

  ws.on("message", (data, isBinary) => {
    if (isBinary) {
      ws.send(data);
      return;
    }
    const text = data.toString("utf8");
    let reply = null;
    try {
      const o = JSON.parse(text);
      if (o && o.type === "ping") {
        reply = JSON.stringify({ type: "pong", t: Date.now() });
      }
    } catch {
      /* not JSON — echo as plain text */
    }
    if (reply !== null) {
      ws.send(reply);
    } else {
      ws.send(text);
    }
  });

  ws.on("close", (code) => {
    // eslint-disable-next-line no-console
    console.log(`[websocket-example] client closed code=${code}`);
  });
});
