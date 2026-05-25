# UI simulator regression loop

Scenarios live in `tests/ui/scenarios/*.json`.

Run:

```bash
./scripts/test-ui-sim.sh
```

Update approved baselines only when the visual change is intentional:

```bash
./scripts/update-ui-baselines.sh
```

Artifacts from failing runs are written to `tests/ui/artifacts/`.
