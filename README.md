# Seta GX

**_Seta GX_** is an experimental Sega Saturn emulator for the Wii. The original Yabause Wii port was heavily modified and simplified. A couple of improvements (namely CHD support and SCU emulation) come from devmiyax's [Yaba Sanshiro](https://github.com/devmiyax/yabause). The main speed improvements comes from hardware accelerated rendering by using some tricks of the Hollywood GPU and a new SH2 JIT recompiler for PPC.

This software is licensed under the GNU General Public License v2, available at: http://www.gnu.org/licenses/gpl-2.0.txt This requires any released modifications to be licensed similarly, and to have the source available.

**DISCLAIMER:** This project is experimental and in no way close to good in terms of code quality and stability (this is to be expected), so you may find a lot of bugs/crashes/softlocks.

### Features
- CUE/BIN or CHD file support.
- GameCube, Wiimote, Classic, Wii U Pro, N64 controller support.
- SD and/or USB storage.
- FPS counter option.
- Native Video option: Makes the Wii follow the same native video configuration set by the Saturn (240p/480i for NTSC and 256p/512i for PAL).


## Installation and configuration

Download and extract the latest release. Copy the `apps/SetaGX` folder into the `apps` folder located on the root of your USB/SD card. Default paths explained below will be serched for both USB and SD card automatically so be sure to create these beforehand (Note: the paths are case sensitive).

| Item   | Default directory    | Notes   |
|--------|----------------------|---------|
| Games  | `vgames/Saturn`      | Game backups should be BIN/CUE or CHD files.   |
| Saves  | `saves/Saturn`       | filename match the game's ID.     |
| BIOS   | `apps/SetaGX`        | **In order to start the emulator, you need to provide your own Saturn BIOS.** The BIOS should be named `bios.bin` and placed in this folder. |
| Covers | `covers/Saturn`      | Must be a PNG and have the same name as the file found in the games folder. The width must be 128px and the height must be 216px or less. |

An optional config file can be setup, found in `apps/SetaGX` and named `setagx.cfg`. In it you can specify custom directory paths for all items in the table above, if an item is not specified in the config file then the default is used. Please see the example config file added in the compressed release files.

## Controller Mapping

Remapping is planned, but the following is the standard mapping:

| Sega Saturn | GameCube  | WiiMote | Classic |  N64  | Wii U / Pro  |
|-------------|-----------|---------|---------|-------|--------------|
| D-pad       | D-pad     | D-pad   | D-pad   | D-pad | D-pad        |
| A           | B         | A       | Y       | A     | Y            |
| B           | A         | 1       | B       | C-V   | B            |
| C           | X         | 2       | A       | C->   | A            |
| X           | Cstick-UP | -       | X       | B     | X            |
| Y           | Y         | B       | ZL      | C-<   | ZL           |
| Z           | Z         |         | ZR      | C-^   | ZR           |
| R           | R         |         | R       | R     | R            |
| L           | L         |         | L       | L     | L            |
| Start       | Start     | +       | +       | Start | +            |
|-------------|-----------|---------|---------|-------|--------------|
| **EXIT**    | Start + Z | Home    | Home    | Z + L | Home         |

To return to the menu press the **EXIT** button(s) at the same time, this will close the game so save before doing this. In the menu you can select a game and press A/2 to start it, pressing Start/+ will open/close the options menu and pressing B/1 will return to the Homebrew Channel.

## Credits/Special Thanks

- Yabause Team: Original soure code
- Devmiyax: Updates to Yabause trough Yaba Sanshiro
- Extrems: Help on GX issues and libogc2
- emu_kidid & Pcercuei: Their continued work on WiiSX helped inspire this endeavor
- tueidj: Virtual memory code
