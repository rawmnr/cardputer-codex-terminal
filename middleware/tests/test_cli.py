from __future__ import annotations

import unittest

from cardputer_codex_terminal.cli import build_parser


class CliTests(unittest.TestCase):
    def test_parser_defaults_match_the_current_scaffold(self) -> None:
        parser = build_parser()
        args = parser.parse_args([])

        self.assertEqual(args.host, "127.0.0.1")
        self.assertEqual(args.port, 8765)
        self.assertEqual(args.codex_ws_url, "ws://127.0.0.1:9000")
        self.assertFalse(args.real_codex)
        self.assertEqual(args.prompt, "Hello Codex, start.")

