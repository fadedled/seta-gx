# Seta GX

**_Seta GX_** is an experimental Sega Saturn emulator for the Wii. The original Yabause Wii port was heavily modified and simplified. A couple of improvements (namely CHD support and SCU emulation) come from devmiyax's [Yaba Sanshiro](https://github.com/devmiyax/yabause). The main speed improvement comes from hardware accelerated rendering by using some tricks of the Hollywood GPU.

This software is licensed under the GNU General Public License v2, available at: http://www.gnu.org/licenses/gpl-2.0.txt This requires any released modifications to be licensed similarly, and to have the source available.

**DISCLAIMER:** This project is experimental and in no way close to good in terms of code quality and stability (this is to be expected), so you may find a lot of bugs/crashes/in

## Features

- CUE or CHD file support.
- GameCube controller support.
- SD and/or USB storage (FAT32).
- Can save games.

## Installation

Download and extract the latest release. Copy the `apps/SetaGX` folder into the `apps` folder located on the root of your USB/SD card. Create a `vgames/Saturn` and a `saves/Saturn` folder where you can place your backups and saves, respectively.

> **BIOS NOT INCLUDED:** In order to start the emulator, you need to provide your own BIOS. Place a Saturn BIOS file inside the `apps/SetaGX/` folder with the name `bios.bin`.

## Controller Mapping

Remapping is planned, currently only the GameCube Controller is supported:

| Sega Saturn | GameCube  |
|-------------|-----------|
| D-pad       | D-pad     |
| A           | B         |
| B           | A         |
| C           | Cstick-UP |
| X           | Y         |
| Y           | X         |
| Z           | Z         |
| R           | R         |
| L           | L         |
| Start       | Start     |

To return to the menu press Start + Z, this will close the game so remember to save before doing this. In the menu you can select a game and press A to start it, by pressing B you return to the Homebrew Channel.

## Credits/Special Thanks

- Yabause Team: Original soure code
- Devmiyax: Updates to Yabause trough Yaba Sanshiro
- Extrems: Help on GX issues
- emu_kidid & Pcercuei: Their continued work on WiiSX helped inspire this endeavor
- tueidj: Virtual memory code
