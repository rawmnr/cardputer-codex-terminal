# Security Policy

Ce projet manipule potentiellement des acces distants a une machine de developpement. Toute implementation devra traiter les secrets, approvals et logs comme des composants critiques.

## Signalement

Pendant la phase de cadrage, ouvrir une issue GitHub avec le label `security` pour signaler une faiblesse de conception.

## Exigences Initiales

- Aucun secret en clair dans le depot.
- Pas d'exposition publique directe du serveur Codex.
- Approbation explicite pour les actions destructrices.
- Documentation de toute hypothese de securite.

