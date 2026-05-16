# Securite

## Surfaces Sensibles

- Cle d'authentification overlay VPN.
- Jeton eventuel du Codex app-server.
- Prompts utilisateur.
- Sorties Codex.
- Commandes shell proposees par Codex.
- Acces aux fichiers du workspace Windows.

## Principes

- Ne pas exposer Codex directement sur Internet.
- Preferer loopback entre middleware et Codex.
- Utiliser l'overlay VPN pour l'acces distant.
- Demander confirmation physique pour les actions critiques.
- Reduire les logs par defaut.
- Prevoir une procedure de revocation des cles.

## Approbations

Les approvals sont un point central du design. Une action destructive ou intrusive doit etre lisible sur l'ecran du Cardputer avant validation.

## Questions Ouvertes

- Niveau de detail affichable sur petit ecran pour les commandes longues.
- Politique de timeout.
- Mode verrouillage en cas de perte ou vol du Cardputer.
- Chiffrement local des secrets sur l'ESP32-S3.

