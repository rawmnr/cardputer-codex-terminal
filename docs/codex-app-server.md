# Integration Codex App Server

## Transport

Le middleware cible une connexion WebSocket locale vers :

```text
codex app-server --listen ws://127.0.0.1:PORT
```

Si le serveur est expose hors loopback, l'authentification par jeton et la restriction reseau deviennent obligatoires.

## Cycle De Session

1. Ouvrir le WebSocket.
2. Envoyer `initialize` avec les capacites client.
3. Envoyer `initialized`.
4. Creer ou reprendre un thread.
5. Envoyer les prompts via `turn/start`.
6. Ecouter les notifications et deltas.
7. Relayer les approvals vers le Cardputer.

## Evenements A Relayer

- Debut et fin d'items.
- Deltas de message agent.
- Statut du tour.
- Requetes d'approbation.
- Erreurs transport ou protocole.

## Politique D'approbation

Le Cardputer doit afficher une demande concise et permettre :

- approbation explicite ;
- refus explicite ;
- timeout configurable ;
- trace minimale cote middleware.

