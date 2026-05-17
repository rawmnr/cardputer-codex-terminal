from __future__ import annotations

import argparse
import asyncio
import json
import sys
from ipaddress import ip_address

from .config import AppConfig
from .core import MiddlewareApp
from .mcp import CardputerMcpServer
from .messages import CardputerMessage, CardputerMessageType
from .preview import DevPreviewServer
from .server import CardputerBridgeServer


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="cardputer-codex-middleware")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--codex-transport", choices=("mock", "stdio", "websocket"), default="mock")
    parser.add_argument("--codex-ws-url", default="ws://127.0.0.1:9000")
    parser.add_argument("--codex-command", default="codex", help="Command used for stdio Codex app-server.")
    parser.add_argument("--workspace", default=".")
    parser.add_argument("--branch", default=None)
    parser.add_argument("--thread-id", default=None)
    parser.add_argument("--real-codex", action="store_true", help="Use the stable stdio Codex app-server transport.")
    parser.add_argument("--prompt", default="Hello Codex, start.")
    parser.add_argument("--serve", action="store_true", help="Run the Cardputer WebSocket bridge instead of a one-shot prompt.")
    parser.add_argument("--preview", action="store_true", help="Run the local browser preview and mirror files.")
    parser.add_argument("--preview-host", default="127.0.0.1")
    parser.add_argument("--preview-port", type=int, default=8787)
    parser.add_argument("--bridge-token", default=None, help="Shared token required by the Cardputer bridge.")
    parser.add_argument("--mcp", action="store_true", help="Run as a stdio MCP server for Codex and keep the bridge listener alive.")
    return parser


def _is_loopback_host(host: str) -> bool:
    if host in {"localhost", "::1"}:
        return True
    try:
        return ip_address(host).is_loopback
    except ValueError:
        return False


async def run_async(args: argparse.Namespace) -> int:
    if args.serve and not _is_loopback_host(args.host) and not args.bridge_token:
        raise SystemExit("Refusing to expose the Cardputer bridge on a non-loopback host without --bridge-token.")
    if args.mcp and args.real_codex:
        raise SystemExit("--mcp runs the middleware as a Codex-side MCP server and cannot be combined with --real-codex.")

    codex_transport = "mock" if args.mcp else ("stdio" if args.real_codex else args.codex_transport)
    app = MiddlewareApp(
        AppConfig(
            host=args.host,
            port=args.port,
            codex_ws_url=args.codex_ws_url,
            codex_transport=codex_transport,
            codex_command=(args.codex_command, "app-server"),
            use_mock_codex=codex_transport == "mock",
            bridge_token=args.bridge_token,
            workspace_path=args.workspace,
            branch=args.branch,
            thread_id=args.thread_id,
        )
    )
    loop = asyncio.get_running_loop()
    preview: DevPreviewServer | None = None
    stream = sys.stderr if args.mcp else sys.stdout
    if args.preview:
        preview = DevPreviewServer(app, args.preview_host, args.preview_port, args.workspace)
        preview.bind_loop(loop)
        preview.start()
        print(f"Preview UI listening on http://{args.preview_host}:{args.preview_port}", file=stream)

    await app.initialize()

    if args.mcp:
        import websockets  # type: ignore

        bridge = CardputerBridgeServer(app)
        mcp_server = CardputerMcpServer(app)

        async def handler(websocket: object, *_: object) -> None:
            await bridge.handle_connection(websocket)

        try:
            async with websockets.serve(handler, args.host, args.port):
                print(f"Cardputer bridge listening on ws://{args.host}:{args.port}", file=stream)
                await mcp_server.run_stdio()
        finally:
            if preview is not None:
                preview.close()
        return 0

    if args.serve:
        import websockets  # type: ignore

        bridge = CardputerBridgeServer(app)

        async def handler(websocket: object, *_: object) -> None:
            await bridge.handle_connection(websocket)

        try:
            async with websockets.serve(handler, args.host, args.port):
                print(f"Cardputer bridge listening on ws://{args.host}:{args.port}")
                if args.prompt:
                    events = await app.handle_cardputer_message(
                        CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": args.prompt})
                    )
                    for event in events:
                        print(json.dumps(event.to_dict(), ensure_ascii=False))
                await asyncio.Future()
        finally:
            if preview is not None:
                preview.close()
        return 0

    if args.prompt:
        events = await app.handle_cardputer_message(
            CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": args.prompt})
        )
        for event in events:
            print(json.dumps(event.to_dict(), ensure_ascii=False))

    if preview is not None:
        try:
            await asyncio.Future()
        finally:
            preview.close()
        return 0

    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return asyncio.run(run_async(args))


if __name__ == "__main__":
    raise SystemExit(main())
