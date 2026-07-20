# Credule 🤪

**Credule** est un fork complètement WTF, chaotique et rigoureusement insupportable du homebrew *Prelude*. 
Tout comme l'original, il permet de basculer la Nintendo Switch entre les serveurs officiels de Nintendo et le réseau alternatif **Nextendo**, mais avec une esthétique absolument désastreuse et des fonctionnalités absurdes.

```
       QUEL RÉSEAU VA FONDRE TON CERVEAU ?
      [ NEXTENDO ]           [ NINTENDO ]
```

## Fonctionnalités WTF de Credule

- 🔴 **Couleurs Néon Agressives** : Un thème visuel aux couleurs criardes clignotantes conçu pour te brûler la rétine.
- 🫨 **Secousses d'Écran Permanentes** : Toute l'interface tremble en permanence. Appuyer sur un bouton provoque un séisme de forte magnitude sur l'écran.
- 📳 **Vibrations Manettes Intenses** : Les moteurs de vibrations de tes Joy-Cons / Manette Pro s'affolent en synchronisation avec les secousses de l'interface.
- 🌐 **Choix Linguistique Incohérent** :
  - **Broken English** : Un anglais approximatif et passif-agressif.
  - **Russe Gopnik** : Du cyrillique écrit en argot de banlieue russe.
  - **Table d'Enchantement Minecraft** : Vos textes traduits dynamiquement en alphabet galactique standard (glyphes grecs).
- 🚨 **Bouton Panique "CALL NINTENDO"** :
  - Une hotline directe vers le service juridique de Nintendo.
  - Lance un compte à rebours de distance ultra-stressant (de 9000 km à 0 m) pendant que la console tremble et vibre à 100%.
  - À 0 m, la musique se coupe pour lancer un sifflement strident (`sfx1.mp3`) et affiche un **faux écran de bannissement système officiel (Error Code: 666-666)**.
- ⚡ **Chargement Instantané** : Suppression de tous les appels réseau synchrones au démarrage qui faisaient freezer l'application pendant 20 secondes. Démarrage en moins de 50ms !

---

## Fonctionnement technique (Sous le capot)

Credule écrit des configurations lues par Atmosphère au redémarrage :

| Quoi | Où |
| --- | --- |
| Redirections d'hôtes | `/atmosphere/hosts/{sysmmc,emummc}.txt` |
| Activation DNS.mitm | `/atmosphere/config/system_settings.ini` |
| PRODINFO masqué | `/exosphere.ini` (seulement en emuMMC) |
| Patchs SSL & Certificats | Dossiers `exefs_patches/`, `nro_patches/` et bundle CA |

---

## Compilation

Si tu as installé la suite devkitPro (avec `switch-freetype switch-sdl2 switch-sdl2_mixer switch-mpg123`) :

```sh
make
```

Le fichier compilé est `credule.nro`. Copie-le dans `/switch/` sur ta carte SD.

---

## Licence

Licencié sous la licence **GNU Affero General Public License, version 3 ou ultérieure**. Voir le fichier [LICENSE](LICENSE) pour plus de détails.
*Ce projet n'est pas affilié à Nintendo. À utiliser à tes propres risques.*
