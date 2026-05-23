from __future__ import annotations

import argparse
import asyncio
import json
import socket
import sys
from ipaddress import ip_address

try:
    from zeroconf import IPVersion, ServiceInfo
    from zeroconf.asyncio import AsyncZeroconf
    HAS_ZEROCONF = True
except ImportError:
    HAS_ZEROCONF = False

from .ble_bridge import CardputerBleBridge, HAS_BLEAK
from .config import AppConfig
from .core import MiddlewareApp
from .mcp import CardputerMcpServer
from .messages import CardputerMessage, CardputerMessageType
from .persistence import DEFAULT_STATE_PATH
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
    parser.add_argument("--serve", action="store_true", help="Run the Cardputer bridge instead of a one-shot prompt.")
    parser.add_argument("--bridge-transport", choices=("wifi", "ble", "hybrid"), default="wifi", help="Bridge transport mode.")
    parser.add_argument("--preview", action="store_true", help="Run the local browser preview and mirror files.")
    parser.add_argument("--preview-host", default="127.0.0.1")
    parser.add_argument("--preview-port", type=int, default=8787)
    parser.add_argument("--bridge-token", default=None, help="Shared token required by the Cardputer bridge.")
    parser.add_argument("--mcp", action="store_true", help="Run as a stdio MCP server for Codex and keep the bridge listener alive.")
    parser.add_argument("--state-path", default=None, help="Path to local run persistence JSON file.")
    return parser


def _is_loopback_host(host: str) -> bool:
    if host in {"localhost", "::1"}:
        return True
    try:
        return ip_address(host).is_loopback
    except ValueError:
        return False


async def run_async(args: argparse.Namespace) -> int:
    if (args.serve or args.mcp) and not _is_loopback_host(args.host) and not args.bridge_token:
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
            state_path=args.state_path or str(DEFAULT_STATE_PATH),
        )
    )
    loop = asyncio.get_running_loop()
    preview: DevPreviewServer | None = None
    stream = sys.stderr if args.mcp else sys.stdout

    def log(message: str) -> None:
        print(message, file=stream, flush=True)

    if args.preview:
        preview = DevPreviewServer(app, args.preview_host, args.preview_port, args.workspace)
        preview.bind_loop(loop)
        preview.start()
        log(f"Preview UI listening on http://{args.preview_host}:{args.preview_port}")

    await app.initialize()

    try:
        if args.mcp or args.serve:
            bridge = CardputerBridgeServer(app)
            mcp_server = CardputerMcpServer(app) if args.mcp else None
            bridge_transport_is_wifi = args.bridge_transport in {"wifi", "hybrid"}
            bridge_transport_is_ble = args.bridge_transport in {"ble", "hybrid"}

            if bridge_transport_is_ble and not HAS_BLEAK:
                raise SystemExit("BLE transport requires the bleak dependency.")

            async def run_wifi_bridge() -> None:
                import websockets  # type: ignore

                zc: AsyncZeroconf | None = None
                local_ip = "127.0.0.1"
                mdns_registered = False
                try:
                    if HAS_ZEROCONF:
                        from zeroconf import NonUniqueNameException

                        zc = AsyncZeroconf(ip_version=IPVersion.V4Only)
                        try:
                            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                                sock.connect(("8.8.8.8", 80))
                                local_ip = sock.getsockname()[0]
                        except Exception:
                            pass

                        service_name = f"Cardputer Codex Terminal ({args.port})._cardputer-codex._tcp.local."
                        info = ServiceInfo(
                            "_cardputer-codex._tcp.local.",
                            service_name,
                            addresses=[socket.inet_aton(local_ip)],
                            port=args.port,
                            server=f"cardputer-codex-{args.port}.local.",
                        )
                        try:
                            await zc.async_register_service(info)
                        except NonUniqueNameException:
                            log(f"Warning: mDNS name {service_name} is already taken, skipping registration.")
                        else:
                            mdns_registered = True

                    async def handler(websocket: object, *_: object) -> None:
                        await bridge.handle_connection(websocket)

                    async with websockets.serve(handler, args.host, args.port):
                        log(f"Cardputer bridge listening on ws://{args.host}:{args.port}")
                        if mdns_registered:
                            log(f"mDNS service registered: {local_ip}:{args.port} (_cardputer-codex._tcp.local.)")
                        await asyncio.Future()
                finally:
                    if zc is not None:
                        await zc.async_unregister_all_services()
                        await zc.async_close()

            async def run_ble_bridge() -> None:
                ble_bridge = CardputerBleBridge(app, logger=log)
                await ble_bridge.run_forever()

            async def run_mcp_stdio() -> None:
                assert mcp_server is not None
                await mcp_server.run_stdio()
                raise RuntimeError("MCP stdio exited unexpectedly.")

            async with asyncio.TaskGroup() as task_group:
                if bridge_transport_is_wifi:
                    task_group.create_task(run_wifi_bridge())
                if bridge_transport_is_ble:
                    task_group.create_task(run_ble_bridge())
                if args.mcp:
                    task_group.create_task(run_mcp_stdio())

                if args.prompt and not args.mcp:
                    events = await app.handle_cardputer_message(
                        CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": args.prompt}, id="msg-000001")
                    )
                    for event in events:
                        print(json.dumps(event.to_dict(), ensure_ascii=False), file=stream, flush=True)

                await asyncio.Future()

        else:
            if args.prompt:
                events = await app.handle_cardputer_message(
                    CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": args.prompt}, id="msg-000001")
                )
                for event in events:
                    print(json.dumps(event.to_dict(), ensure_ascii=False), file=stream, flush=True)

            if preview is not None:
                await asyncio.Future()
            return 0
    finally:
        if preview is not None:
            preview.close()
        await app.close()


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return asyncio.run(run_async(args))


if __name__ == "__main__":
    raise SystemExit(main())
