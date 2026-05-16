# Architecture

## Objectif

Construire une interface physique mobile pour OpenAI Codex, basee sur le M5Stack Cardputer ADV, capable de piloter une session Codex distante executee sur une machine Windows.

## Strates

1. **Firmware embarque**
   - Capture clavier.
   - Capture audio push-to-talk.
   - Affichage du flux agentique.
   - Transport WebSocket vers le middleware.
   - Connexion reseau securisee via overlay VPN.

2. **Middleware Windows**
   - Serveur WebSocket pour le Cardputer.
   - Reception audio PCM.
   - Transcription speech-to-text.
   - Client JSON-RPC pour Codex app-server.
   - Routage des evenements Codex vers l'ecran du Cardputer.

3. **Codex app-server**
   - Gestion des threads.
   - Execution des tours.
   - Streaming des messages.
   - Gestion des approvals.
   - Acces aux outils locaux et MCP.

## Flux Principaux

### Prompt Clavier

```text
Cardputer keyboard -> middleware -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Prompt Vocal

```text
Cardputer microphone -> PCM chunks -> middleware STT -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Approval

```text
Codex approval request -> middleware -> Cardputer alert -> user keypress -> middleware -> Codex approval response
```

## Principes

- Maintenir l'etat conversationnel cote Codex, pas sur le microcontroleur.
- Garder le Cardputer comme terminal leger, robuste et reactif.
- Eviter l'exposition publique directe du serveur Windows.
- Separar clairement transport, transcription et protocole Codex.

