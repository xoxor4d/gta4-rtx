# Installation:

### Install using the installer:
1. Download <LINK_TO_MOD_ZIP>
2. Download <LINK_TO_INSTALLER>

3. Place both files in the same folder (_no need to copy them to your game folder_) and run `GTAIV-Remix-CompMod-Installer.exe` 
4. Use the File Dialog to select your `GTAIV.exe` which is located in your GTAIV install folder

5. If this is your first time installing, the installer will ask if you want to install a custom Fork of [FusionFix](https://github.com/xoxor4d/GTAIV.EFLC.FusionFix.RTXRemix/tree/feature/rtx-remix-rebase1)  
    - Press YES - If you already have FusionFix installed, it's definitely recommended to install this because the original, unmodified version has a few incompatibilities with RTX Remix. If you do not have FusionFix installed, you are free to choose. It's not a requirement but highly recommended.

6. The installer will ask you if you want to download the required [base-remix-mod](https://github.com/xoxor4d/gta4-rtx-base-mod).  
    - Press YES - The CompMod will not function correctly without it - so make sure to install it.
    - Use the `GIT` install method (recommended) because its easier and faster to update to new versions (and uses less bandwidth)

7. The installer will ask you if you want to download the optional [gta4-autopbr-mod](https://github.com/xoxor4d/gta4-rtx-autopbr-mod).
    - This mod converts a bunch of game materials to approximated PBR-materials and makes the game less plasticy. It is entirely optional.

8. Make sure that you remove all custom launch arguments for GTAIV in Steam (if you have any set and use Steam to run the game)

<br>

> [!IMPORTANT]
> If you want to use DLSS 5 (NR), you'll need to source and place `nvngx_dlssnr.dll` into the `.trex` folder on your own.  
> You can then use the NR settings found within the Post-Processing tab (Alt+X - Advanced Menu) 

<br>

> [!TIP]
> You can re-run the installer at any time and choose to only check for **remix-base-mod** or **AutoPBR** updates.  
> If you've used the `GIT` installation method, it will do delta updates and pull the latest changes. 
> The base mod (textures/meshes) is getting constant updates which are not coupled to Compatibility Mod releases.

<br>

### OR Install manually (no need to do this if you've used the installer):
1. Download <LINK_TO_MOD_ZIP>

3. Open the zip and extract all files contained inside the `GTAIV-Remix-CompatibilityMod` folder into your GTAIV directory (next to the `GTAIV.exe`). Overwrite all when prompted.
4. If you want to use FusionFix or have it installed already, it's definitely recommended to install rtx-remix fork of it.  
You can find the files inside `_installer_options/FusionFix_RTXRemixFork`. Extract the `plugins` & `update` into your GTAIV directory and override any existing files.  
You may also want to use `GTAIV.EFLC.FusionFix.cfg` from either `mode_fullscreen/plugins` or `mode_windowed/plugins`.

5. Download [gta4-rtx-base-mod](https://github.com/xoxor4d/gta4-rtx-base-mod/archive/refs/heads/master.zip) 
6. Extract the _mods_ folder (inside of `gta4-rtx-base-mod-master`) into your `rtx-remix` folder so that the folder structure looks like this:

```
.  
├─ ...
├─ 📁 steamapps
│  └─📁 common
│     └─📁 Grand Theft Auto IV
│       └─📁 GTAIV
│         ├── 📜 a_gta4-rtx.asi
│         ├── 📜 d3d9.dll
│         ├── 📜 GTAIV.exe
│         ├── 📜 ...
│         │
│         ├── 📁 rtx_comp
│         └── 📁 rtx-remix
│             └─📁 mods
│               └─📁 gta4rtx
│                 ├── 📜 comp_cars.usda
│                 ├── 📜 comp_effects.usda
│                 ├── 📜 mod.usda
│                 └── ...
└── ...  
```

<br>

# Credits
- [NVIDIA - RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix)
- [People of the showcase discord](https://discord.gg/j6sh7JD3v9) - especially the nvidia engineers ✌️
- [Dear ImGui](https://github.com/ocornut/imgui)
- [imgui-blur-effect](https://github.com/3r4y/imgui-blur-effect)
- [minhook](https://github.com/TsudaKageyu/minhook)
- [toml11](https://github.com/ToruNiina/toml11)
- [Ultimate-ASI-Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [miniz](https://github.com/richgel999/miniz)
- [Rapidjson](https://github.com/Tencent/rapidjson)
- [DiscordRPC](https://github.com/discord/discord-rpc)
- [FusionFix](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix)
- [FusionShaders](https://github.com/Parallellines0451/GTAIV.EFLC.FusionShaders)
- [Rage-Shader-Editor](https://github.com/ImpossibleEchoes/rage-shader-editor-cpp)
- [IV-SDK](https://github.com/Zolika1351/iv-sdk/)
- [IV-SDK-DotNet](https://github.com/ClonkAndre/IV-SDK-DotNet)
- [AssaultKifle47](https://github.com/akifle47)
- [DayL](https://www.gtainside.de/de/user/falcogray)
- [Entity](https://www.youtube.com/@paprykszadolowski8796)
- [Gabdeg](https://www.youtube.com/@gabdeg793)
- [Hemry](https://www.youtube.com/@Hemry81)
- [Danlopand / Thundery_Dan](https://github.com/DANLOPAND)
- [KapibosRU](https://www.youtube.com/channel/UCqZ2NI_fQKRN-Onypt9aIGQ)
- [Budgie](https://www.patreon.com/c/BudgieGames)
- [Alex from Digital Foundry](https://www.youtube.com/watch?v=vGxPdcMQfwg)
- [Sparkles (Remix Plus - Numos - DLSS5 Integration)](https://github.com/Kim2091)
- [CR (Remix Plus)](https://github.com/sambow23)
- [TheGreatHMMMM (Remix Plus)](https://github.com/TheGreatHMMMM)
- [Gokuwashere (Remix Plus)](https://github.com/BrunchyChineapple)
- All 🍓 Testers

<br>

# Usage and general Info
- Run the game like normal or use the provided batch files (mentioned in the Wiki further down) if you notice heavy stuttering.
> - Press `Alt + X` to open the Remix menu  
> - Press `F4` to open the Compatibility Mod menu

### Toggling the Sky System
- Press `F4` and toggle `Use Remix Atmosphere System`

<br>
<br>

> [!NOTE]  
> **Troubleshooting / Guides** -- Look into the **Wiki** if you are having issues:  
> https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides

> The release includes a custom remix runtime build that contains a few necessary changes. Info about the changes can be found here:  
> https://github.com/xoxor4d/gta4-rtx/wiki#remix-runtime-changes-and-differences-in-usage

---

> [!Important]
> Installer False-Positive: If you get a false positive on the installer, download and use the Installer that shipped with  
[1.3.2](https://github.com/xoxor4d/gta4-rtx/releases/download/v1.3.2/GTAIV-Remix-CompMod-Installer.exe) or the one shipped with [1.4.0](https://github.com/xoxor4d/gta4-rtx/releases/download/v1.4.0/GTAIV-Remix-CompMod-Installer.exe)

