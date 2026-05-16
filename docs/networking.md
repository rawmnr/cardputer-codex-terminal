# Reseau Distant

## Besoin

Le Cardputer doit joindre une machine Windows distante qui execute Codex, meme derriere NAT ou pare-feu domestique.

## Options

| Option | Avantage | Limite |
| --- | --- | --- |
| Port forwarding | Simple en theorie | Expose l'hote, fragile avec CGNAT |
| Tunnel inverse | Rapide pour prototype | Depend d'un tiers, latence, limites gratuites |
| Overlay VPN | Stable et securise | Integration embarquee plus complexe |

## Direction Cible

Utiliser un overlay VPN type Tailscale avec une integration embarquee compatible ESP32, par exemple MicroLink.

## Topologie

```text
Cardputer -> Wi-Fi -> overlay VPN -> IP privee tailnet Windows -> middleware Python
```

## Decisions A Valider

- Bibliotheque VPN embarquee retenue.
- Strategie de provisionnement de la cle d'authentification.
- Rotation et revocation des secrets.
- Mode degrade sans VPN pour developpement local.

