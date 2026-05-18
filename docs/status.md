# État actuel du projet (mise à jour 2026-05-18)

Ce document résume l'état du dépôt cardputer-codex-terminal au moment de la mise à jour. Il est conçu pour compléter la documentation existante en indiquant ce qui est déjà implémenté, comment construire et tester le projet localement, et quelles sont les limitations connues / tâches prioritaires.

## Résumé rapide

- Middleware (Windows, Python) : structure et CLI présentes dans `middleware/`. Mode `--serve` pour le pont WebSocket et mode `--mcp` pour l'interface de tooling over stdio. Transport Codex recommandé : `StdioCodexAppServerTransport`.
- Firmware (M5 Cardputer ADV) : code source dans `firmware/src/` avec gestion clavier, push-to-talk, capture micro (I2S/PDM), rendu écran et lien vers le middleware. Le firmware produit une `.bin` flashable via PlatformIO.
- Intégration : le firmware dialogue avec le middleware via des enveloppes JSON versionnées (protocol version = 1). Le middleware conserve l'index de sessions et renvoie des snapshots pour la Pager app.

## Ce qui est implémenté (vérifié dans le dépôt source)

- Commandes et shell firmware : app shell, navigation des apps, commandes `/help`, `/app`, `/wifi`, `/status`, etc.
- Push-to-talk : logique de tenue d'appui (hold) sur `Space`, démarrage et arrêt de l'enregistrement, capture micro via M5.Mic (16000 Hz) et envoi d'événements vers le middleware.
- Bridge firmware : configuration via `/cardputer-codex/config.ini` sur la carte SD, support de `bridge_token` pour sécuriser le serveur middleware si exposé.
- Pager et MCP : hooks pour afficher approbations, notifications et sélection de réponses sur la Cardputer ; le middleware expose des outils `cardputer.notify`, `cardputer.ask`, `cardputer.confirm`, `cardputer.show`.
- Tests middleware : tests unitaires disponibles sous `middleware/tests/` (framework `unittest`).

## Commandes de build & d'exécution (rapide)

- Middleware
  - uv sync
  - uv run cardputer-codex-middleware --help
  - uv run cardputer-codex-middleware --serve
  - uv run cardputer-codex-middleware --serve --bridge-token <shared-secret>
  - uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
  - uv run python -m unittest discover -s tests -v

- Firmware
  - cd firmware
  - python -m platformio run
  - binaire attendu : `firmware/cardputer-codex-terminal.bin`

## Limitations connues et points d'attention

- VPN/overlay : l'intégration d'un overlay VPN (Tailscale/MicroLink) reste une décision à finaliser et n'est pas fournie clé en main.
- Transcription : pipeline voix → STT (faster-whisper ou équivalent) est décrit dans la doc middleware mais dépend d'implémentations et de modèles locaux qui ne sont pas livrés ici.
- Sécurité : la meilleure pratique est d'utiliser `bridge_token` si le middleware est écouté sur une interface non-loopback. Ne pas exposer directement le serveur Codex.
- UI texte : l'affichage sur l'écran 240×135 doit rester concis — la documentation UX le rappelle mais les limites exactes d'affichage long ne sont pas définies.
- Tests locaux : si PlatformIO ou `uv` sont absents, la compilation/exécution n'a pas été vérifiée ici.

## Tâches prioritaires recommandées

1. Valider et documenter la stratégie VPN/provisionnement des clés (MicroLink/Tailscale) pour accès hors-LAN.
2. Ajouter tests automatisés couvrant le flux push-to-talk end-to-end en simulation (mock transport).
3. Regénérer et ajouter les schémas `codex app-server` après toute mise à jour de Codex (instructions déjà présentes dans docs).
4. Ajouter un guide de flashage pas-à-pas pour M5 Launcher (rappel des prérequis et options de récupération).

## Mise à jour de la documentation

- Ce fichier complète `docs/architecture.md`, `docs/middleware.md` et `docs/firmware.md` en apportant un instantané de l'état actuel. Si vous voulez, je peux :
  - fusionner certains passages directement dans les fichiers existants (ex. étendre `docs/firmware.md` avec les comportements vérifiés),
  - ou créer une PR qui met à jour/normalise les pages existantes et ajoute une checklist de tâches ouvertes.

---

Si vous souhaitez que j'applique ces changements directement dans d'autres fichiers (par exemple mettre à jour `docs/firmware.md` ou `AGENTS.md`), dites-moi exactement quels fichiers modifier et j'exécuterai les commits correspondants.