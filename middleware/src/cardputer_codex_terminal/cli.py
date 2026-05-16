from __future__ import annotations

import argparse
import asyncio
import json

from .config import AppConfig
from .core import MiddlewareApp
from .messages import CardputerMessage, CardputerMessageType
from .server import CardputerBridgeServer


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="cardputer-codex-middleware")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--codex-ws-url", default="ws://127.0.0.1:9000")
    parser.add_argument("--workspace", default=".")
    parser.add_argument("--branch", default=None)
    parser.add_argument("--thread-id", default=None)
    parser.add_argument("--real-codex", action="store_true", help="Use the real Codex transport when implemented.")
    parser.add_argument("--prompt", default="Hello Codex, start.")
    parser.add_argument("--serve", action="store_true", help="Run the Cardputer WebSocket bridge instead of a one-shot prompt.")
    return parser


async def run_async(args: argparse.Namespace) -> int:
    app = MiddlewareApp(
        AppConfig(
            host=args.host,
            port=args.port,
            codex_ws_url=args.codex_ws_url,
            use_mock_codex=not args.real_codex,
            workspace_path=args.workspace,
            branch=args.branch,
            thread_id=args.thread_id,
        )
    )
    await app.initialize()

    if args.serve:
        import websockets  # type: ignore

        bridge = CardputerBridgeServer(app)

        async def handler(websocket: object, *_: object) -> None:
            await bridge.handle_connection(websocket)

        async with websockets.serve(handler, args.host, args.port):
            print(f"Cardputer bridge listening on ws://{args.host}:{args.port}")
            await asyncio.Future()

    events = await app.handle_cardputer_message(
        CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": args.prompt})
    )
    for event in events:
        print(json.dumps(event.to_dict(), ensure_ascii=False))
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return asyncio.run(run_async(args))


if __name__ == "__main__":
    raise SystemExit(main())
