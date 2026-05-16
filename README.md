# cardputer-codex-terminal

Terminal agentique distant pour transformer le M5Stack Cardputer ADV en interface physique de pilotage d'une instance OpenAI Codex executee sur Windows.

Ce depot est initialise comme un projet d'architecture. Il ne contient pas encore de code firmware ou middleware. L'objectif initial est de cadrer les composants, les protocoles, les risques et la feuille de route avant implementation.

## Vision

Le projet vise a relier un M5Stack Cardputer ADV a un hote Windows executant `codex app-server`, afin de fournir une interface mobile pour :

- envoyer des instructions texte depuis le clavier du Cardputer ;
- dicter des requetes vocales via push-to-talk ;
- suivre les reponses Codex en streaming ;
- valider ou refuser les demandes d'approbation ;
- superviser les taches longues a distance via un reseau prive overlay.

## Architecture Cible

```text
M5Stack Cardputer ADV
  | Wi-Fi + overlay VPN
  v
Middleware Python sur Windows
  | WebSocket / JSON-RPC
  v
OpenAI Codex app-server
  | outils locaux / MCP / shell
  v
Workspace Windows
```

## Structure Du Depot

```text
docs/
  architecture.md          Vue d'ensemble systeme
  hardware.md              Notes materiel Cardputer ADV
  networking.md            Acces distant, Tailscale, MicroLink
  middleware.md            Role du pont Python et pipeline STT
  firmware.md              Design du firmware embarque
  codex-app-server.md      Integration JSON-RPC avec Codex
  security.md              Menaces, approbations, secrets
  roadmap.md               Phases de realisation
  references.md            Sources et projets connexes
firmware/                  Futur firmware ESP32-S3
middleware/                Futur serveur Python Windows
hardware/                  Notes, schemas, pinout, assets techniques
```

## Statut

Phase 0 : cadrage du projet et structure du depot.

Le code sera ajoute dans une phase ulterieure, apres validation des choix de transport, de securite et d'experience utilisateur.

