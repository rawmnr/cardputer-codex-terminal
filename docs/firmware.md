# Firmware Cardputer

## Role

Le firmware transforme le Cardputer en terminal physique leger pour Codex.

## Modules Prevus

- Gestion Wi-Fi.
- Integration overlay VPN.
- Client WebSocket.
- Capture clavier.
- Capture audio I2S/PDM.
- Gestion affichage terminal.
- Alertes sonores.
- Gestion batterie et veille.

## Taches FreeRTOS Cibles

| Tache | Responsabilite |
| --- | --- |
| network_task | Connexion, WebSocket, VPN |
| input_task | Clavier et raccourcis |
| audio_task | DMA microphone et push-to-talk |
| display_task | Rendu texte, statut, approvals |
| power_task | Batterie, veille, frequence CPU |

## Interface Utilisateur

- Barre de statut : reseau, batterie, etat Codex.
- Zone principale : flux agentique.
- Ligne de saisie : prompt clavier.
- Ecran approval : action demandee, accepter/refuser.

## Contraintes

- L'ecran 240 x 135 impose un wrapping strict.
- Le scrolling doit eviter les scintillements.
- Le firmware ne doit pas porter la logique Codex complexe.
- Les secrets doivent etre stockes et affiches avec prudence.

