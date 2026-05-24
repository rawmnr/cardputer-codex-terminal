from __future__ import annotations

import os
import tempfile
import unittest
from unittest.mock import patch

from cardputer_codex_terminal.cli import _is_loopback_host, _resolve_bridge_token, build_parser


class CliTests(unittest.TestCase):
    def test_parser_defaults_match_the_current_scaffold(self) -> None:
        parser = build_parser()
        args = parser.parse_args([])

        self.assertEqual(args.host, "127.0.0.1")
        self.assertEqual(args.port, 8765)
        self.assertEqual(args.codex_transport, "mock")
        self.assertEqual(args.codex_ws_url, "ws://127.0.0.1:9000")
        self.assertEqual(args.codex_command, "codex")
        self.assertEqual(args.workspace, ".")
        self.assertIsNone(args.branch)
        self.assertIsNone(args.thread_id)
        self.assertFalse(args.real_codex)
        self.assertFalse(args.serve)
        self.assertEqual(args.bridge_transport, "wifi")
        self.assertFalse(args.preview)
        self.assertEqual(args.preview_host, "127.0.0.1")
        self.assertEqual(args.preview_port, 8787)
        self.assertEqual(args.prompt, "Hello Codex, start.")
        self.assertFalse(args.mcp)

    def test_loopback_detection_for_bridge_safety(self) -> None:
        self.assertTrue(_is_loopback_host("127.0.0.1"))
        self.assertTrue(_is_loopback_host("::1"))
        self.assertTrue(_is_loopback_host("localhost"))
        self.assertFalse(_is_loopback_host("0.0.0.0"))
        self.assertFalse(_is_loopback_host("192.168.1.20"))

    def test_resolve_bridge_token_prefers_env_over_file(self) -> None:
        with tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8") as handle:
            handle.write("file-token\n")
            token_path = handle.name

        try:
            with patch.dict(os.environ, {"CARDPUTER_BRIDGE_TOKEN": "env-token", "CARDPUTER_BRIDGE_TOKEN_FILE": token_path}, clear=False):
                self.assertEqual(_resolve_bridge_token(), "env-token")
        finally:
            os.unlink(token_path)

    def test_resolve_bridge_token_reads_file(self) -> None:
        with tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8") as handle:
            handle.write("file-token\n")
            token_path = handle.name

        try:
            with patch.dict(os.environ, {"CARDPUTER_BRIDGE_TOKEN": "", "CARDPUTER_BRIDGE_TOKEN_FILE": token_path}, clear=False):
                self.assertEqual(_resolve_bridge_token(), "file-token")
        finally:
            os.unlink(token_path)

    def test_parser_accepts_mcp_mode(self) -> None:
        parser = build_parser()
        args = parser.parse_args(["--mcp", "--bridge-transport", "hybrid"])

        self.assertTrue(args.mcp)
        self.assertEqual(args.bridge_transport, "hybrid")