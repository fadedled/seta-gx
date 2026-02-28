# Seta GX

**_Seta GX_** is an experimental Sega Saturn emulator for the Wii. The original Yabause Wii port was heavily modified and simplified. A couple of improvements (namely CHD support and SCU emulation) come from devmiyax's [Yaba Sanshiro](https://github.com/devmiyax/yabause). The main speed improvement comes from hardware accelerated rendering by using some tricks of the Hollywood GPU.

This software is licensed under the GNU General Public License v2, available at: http://www.gnu.org/licenses/gpl-2.0.txt This requires any released modifications to be licensed similarly, and to have the source available.

**DISCLAIMER:** This project is experimental and in no way close to good in terms of code quality and stability (this is to be expected), so you may find a lot of bugs/crashes/softlocks.

## Features

- CUE or CHD file support.
- WiiMote, Classic and GameCube controller support.
- SD and/or USB storage (FAT32).
- Can save games.

## Installation

Download and extract the latest release. Copy the `apps/SetaGX` folder into the `apps` folder located on the root of your USB/SD card. From the root of your USB/SD card, games must be stored in a `vgames/Saturn` directory, saves will be located in a `saves/Saturn` directory, be sure to create these beforehand (Note: the paths are case sensitive).

> **BIOS NOT INCLUDED:** In order to start the emulator, you need to provide your own BIOS. Place a Saturn BIOS file inside the `apps/SetaGX/` folder with the name `bios.bin`.

## Controller Mapping

Remapping is planned, but the following is the standard mapping:

| Sega Saturn | GameCube  | WiiMote | Classic |
|-------------|-----------|---------|---------|
| D-pad       | D-pad     | D-pad   | D-pad   |
| A           | B         | A       | Y       |
| B           | A         | 1       | B       |
| C           | X         | 2       | A       |
| X           | Cstick-UP | -       | X       |
| Y           | Y         | B       | ZL      |
| Z           | Z         |         | ZR      |
| R           | R         |         | R       |
| L           | L         |         | L       |
| Start       | Start     | +       | +       |

To return to the menu press **Start + Z** on GameCube Controller and **Home** on Wii controllers, this will close the game so save before doing this. In the menu you can select a game and press A/2 to start it (if you start a game while holding R an FPS counter will show), pressing B/1 will return to the Homebrew Channel.

## Credits/Special Thanks

- Yabause Team: Original soure code
- Devmiyax: Updates to Yabause trough Yaba Sanshiro
- Extrems: Help on GX issues and libogc2
- emu_kidid & Pcercuei: Their continued work on WiiSX helped inspire this endeavor
- tueidj: Virtual memory code
