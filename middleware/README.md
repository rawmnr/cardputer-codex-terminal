# Middleware

This folder contains the first Windows bridge prototype between the Cardputer and Codex.

The initial scaffold provides:

- a CLI;
- an event model;
- a Codex transport abstraction;
- a mock for early local testing.

The Cardputer message contract is versioned. Current protocol version: `1`.

## Tooling

This package is managed with `uv`.

Set up the environment:

```bash
uv sync
```

Run the CLI:

```bash
uv run cardputer-codex-middleware --help
```

## Tests

Run the middleware tests with:

```bash
uv run python -m unittest discover -s tests -v
```
