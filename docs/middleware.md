# Middleware Windows

## Role

Le middleware sert de pont entre le Cardputer et Codex. Il compense les limites du microcontroleur et isole le protocole Codex du firmware.

## Responsabilites

- Exposer un serveur WebSocket au Cardputer.
- Recevoir les commandes texte.
- Recevoir les fragments audio PCM.
- Transcrire l'audio en texte.
- Ouvrir une session JSON-RPC vers `codex app-server`.
- Demarrer ou reprendre les threads Codex.
- Relayer les deltas et statuts vers le Cardputer.
- Gerer les demandes d'approbation.

## Pipeline Vocal Cible

```text
PCM int16 -> buffer memoire -> VAD optionnel -> resampling 16 kHz si besoin -> faster-whisper -> texte -> turn/start
```

## Contraintes

- Boucle asynchrone non bloquante.
- Pas de stockage audio persistant par defaut.
- Journalisation prudente pour eviter d'ecrire des secrets ou prompts sensibles.
- Mode simulation utile avant firmware reel.

