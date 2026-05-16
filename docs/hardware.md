# Materiel

## Plateforme

Materiel cible : M5Stack Cardputer ADV, base sur ESP32-S3.

## Capacites Pertinentes

- SoC ESP32-S3 avec deux coeurs Xtensa LX7.
- Wi-Fi integre.
- Ecran TFT ST7789V2.
- Clavier physique 56 touches.
- Microphone numerique MEMS PDM.
- Haut-parleur integre.
- Batterie interne et base d'extension.

## Pinout Fonctionnel

| Peripherique | Interface | Broches | Usage Projet |
| --- | --- | --- | --- |
| Microphone SPM1423 | I2S/PDM entree | DAT GPIO 46, CLK GPIO 43 | Capture push-to-talk |
| Haut-parleur NS4168 | I2S sortie | BCLK 41, SDATA 42, LRCLK 43 | Alertes et notifications |
| Ecran ST7789V2 | SPI | CS 37, SCK 36, DAT 35, RST 33, RS 34, BL 38 | Terminal visuel |
| Clavier | Matrice GPIO | interne | Commandes texte et approvals |

## Points D'attention

- La broche GPIO 43 est partagee entre l'horloge microphone et LRCLK haut-parleur.
- La capture audio doit privilegier DMA et buffers circulaires.
- La PSRAM doit etre consideree pour les buffers audio et affichage.
- L'autonomie depend fortement de l'usage Wi-Fi, ecran et chiffrement reseau.

