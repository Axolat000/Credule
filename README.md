# Credule 🤪

**Credule** is a completely WTF, chaotic, and downright obnoxious Nintendo Switch homebrew fork of the original *Prelude*. 
Just like the original, it lets you switch your console between Nintendo's official servers and the alternative **Nextendo** network, but it does so in the most annoying way possible.

```
       WHICH NETWORK WILL MELT YOUR BRAIN?
      [ NEXTENDO ]           [ NINTENDO ]
```

## WTF Features

- 🔴 **Aggressive Neon Colors**: An eye-burning visual theme featuring flashing neon colors designed to fry your retinas.
- 🫨 **Constant Screen Shaking**: The entire UI shakes continuously. Pressing any button triggers a high-magnitude earthquake on the screen.
- 📳 **Violent Controller Rumble**: Your Joy-Cons / Pro Controller rumble motors go wild in sync with the screen shakes.
- 🌐 **Incoherent Language Options**:
  - **Broken English**: Approximative, passive-aggressive English translation.
  - **Gopnik Russian**: Cyrillic translation using Gopnik street slang.
  - **Minecraft Enchanting Table**: Texts dynamically translated into the Standard Galactic Alphabet (Greek glyphs).
- 🚨 **"CALL NINTENDO" Panic Button**:
  - A direct hotline to Nintendo's Legal Department.
  - Triggers an ultra-stressful distance countdown (from 9000 km down to 0 m) while the console shakes and rumbles at 100% power.
  - At 0 m, the background music cuts out to play a screeching sound effect (`sfx1.mp3`) and displays a **perfectly realistic fake system ban screen (Error Code: 666-666)**.
- ⚡ **Instant Boot**: Removed all annoying synchronous network checks on startup that used to freeze the application for 20 seconds. Credule now boots in under 50ms!

---

## Technical Details

Credule writes configuration files that Atmosphère reads at boot:

| What | Where |
| --- | --- |
| Host redirections | `/atmosphere/hosts/{sysmmc,emummc}.txt` |
| DNS.mitm toggle | `/atmosphere/config/system_settings.ini` |
| Hidden PRODINFO | `/exosphere.ini` (emuMMC only) |
| SSL Patches & Certificates | `exefs_patches/`, `nro_patches/` folders and browser CA bundle |

---

## Building

If you have devkitPro installed (with `switch-freetype switch-sdl2 switch-sdl2_mixer switch-mpg123`):

```sh
make
```

The output file is `credule.nro`. Copy it to `/switch/` on your SD card.

---

## License

Licensed under the **GNU Affero General Public License, version 3 or later**. See [LICENSE](LICENSE) for more details.
*This project is not affiliated with Nintendo. Use at your own risk.*
