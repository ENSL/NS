# Natural Selection v3.3

An updated build of the game [Natural Selection] for Windows and Linux focused on quality of life improvements and bug fixes.

## Downloads

The **[Natural Selection Launcher](https://github.com/ENSL/NaturalLauncher/releases/)** (Windows) can install, update, or repair the game.

**[Manual installation](https://github.com/ENSL/NS/releases)** (Windows / Linux)

As the game is a Half-Life mod, Steam and Half-Life installations are required to play the game.

A fresh install of NS comes with updated config files containing everything you need to get playing on the standard settings most players prefer. Customization options are also built in like the Nine Legends competitve UI option as well as team and weapon specific config files that can be edited.

## Game not working? Troubleshooting tips

If the game doesn't load, check the following:
1. If you recieve a "could not load library" error for the client.dll, please install the **[latest Microsoft Visual C++ Redistributable package](https://aka.ms/vs/17/release/vc_redist.x86.exe)**.
1. Make sure you have verified Half-Life's integrity. Click [here](https://support.steampowered.com/kb_article.php?ref=2037-QEUH-3335) for detailed instructions.
1. Check if Half-Life works for you.
1. Make sure you have a clean install. Go to the half-life directory (eg. `c:\Program Files\Steam\steamapps\common\Half-Life`) and remove or rename the ``ns`` folder, then try installing NS.
1. Make sure you don't have any additional command line options for NS.
1. For more help, ask on #help in [the community discord](https://discord.gg/ZUSSBUA)

For Linux:
- Remove or rename the `libstdc++so.6` provided by steam or steamcmd so the linux distro's can be used instead. The one steam provides is outdated.
- 32 bit C libraries might need to be installed. Try `apt-get install libc6-i386` if on debian or ubuntu.

## Changes

Updates include:

- Natural Selection Launcher for installing and updating the game
- Widescreen support (now expands FOV up to 16:9 aspect instead of cropping the image)
- AI upscaled model textures can be turned on with the "Use High Definition models" video option 
- Many FPS dependent bugs fixed, including jetpack acceleration, so the game can now be fairly played at 200+ FPS
- Quake style queued jumping to make bunnyhopping more accessible (server adjustable via sv_jumpmode)
- Shotgun and grenade launcher have been reworked to fix reload bugs
- Weapon reloads are now predicted on the client
- New minimal HUD and the Nine Legends HUD can be selected through advanced options or hud_style and hud_mapstyle
- New crosshair system that can be adjusted through the advanced options and cl_cross commands (Thanks [OpenAG](https://github.com/YaLTeR/OpenAG))
- Ambient sounds can be changed in advanced options or via cl_ambientsound
- Marine HUD now tracks research progress and alien HUD tracks hive growth
- Raw input and sensitivity scaling options now available
- Lerks no longer stick to the ground if they touch it when gliding
- Players can change lifeform and use the popup menu in pregame warmup

- Numerous bug fixes

## Compiling

For Windows, compilation should be working if you have VS2019 installed.

For Linux:

First you need some libraries. On Ubuntu it is:

```sh
apt-get install build-essential git gdb gcc-multilib g++-multilib libc6-i386 mesa-common-dev
``` 

Then you will need to get the files:
```sh
git clone https://github.com/ENSL/NS.git
``` 
Then cd to the linux directory:
```sh
cd NS/main/source/linux
```

Then build
```sh
make
```
or `make ns` to build the server binary and `make cl_dll` for the client binary. `make clean` to clean.

If you get the this error when running the app: `Fatal Error - could not load library (client.so)`, With a high chance it is because of some `UNDEFINED SYMBOLS` in the shared library. But you can check this with this command:

``` sh 
ldd -r -d client.so
``` 

Make sure you have vgui.so copied to the cl_dll folder too on Linux.

## Debugging

Windows debug builds are currently broken.

If you want to debug:
```sh
LD_LIBRARY_PATH=".:$LD_LIBRARY_PATH" gdb ./hl_linux r -game ns -dev -steam
``` 
Due to the new engine and the nature of Linux I had to make a lot of changes! You can find them with grep -Ril `@Linux`.

For MacOS & lldb:
```
DYLD_LIBRARY_PATH=".:$DYLD_LIBRARY_PATH" /Applications/Xcode.app/Contents/Developer/usr/bin/lldb -- ./hl_osx r -game ns -dev -steam -windowed
```
Have to use XCode's lldb to get around https://stackoverflow.com/a/33589760 (Lack of environment variables). `-windowed` as windowed mode makes it easier to deal with crashes on MacOS.

### Hosting a server

Since servers are really rare at this point in time here is a tutorial how to set up an [Natural Selection] server with [hlds]. 

1. Follow these steps to get steamCMD installed and HLDS updated in it: https://developer.valvesoftware.com/wiki/SteamCMD
2. You'll want to run `app_update 90 validate` multiple times in steamCMD to install HLDS and fully update it, as it won't completely do it the first time.
3. Copy the `ns` directory into the `hlds` directory after installing HLDS from steamcmd
4. Run the game : 
```sh
./hlds_run -game ns -autoupdate +map ns_eclipse + sv_secure 1 + port 27015 +hostname "Natural Selection" +maxplayers 32
``` 
If you are behind a NAT(Router) make sure to open at least those ports: 
- 27015 UDP (game transmission, pings) 
- 26900 UDP (VAC service) -- automatically increments if used in case of additional server processes

`Under no circumstances expose your host to the internet!!!` For more information follow [this link](https://developer.valvesoftware.com/wiki/Half-Life_Dedicated_Server).

In order to check if you server is connected to the steam servers copy the following url in your browser and replace `<your IP address>` with your external ip:
`http://api.steampowered.com/ISteamApps/GetServersAtAddress/v0001?addr=<your IP address>&format=json`

There is an updated version of metamod called [metamodp](http://metamod-p.sourceforge.net/). Since players are also pretty rare you might want to install [rcbot] and therefore you have to download those files:
 - [rcbot without dlls](http://filebase.bots-united.com/index.php?act=download&id=210)
 - [rcbot linux dlls](http://filebase.bots-united.com/index.php?act=download&id=428)

### Usefull links

* [Usefull collection of ns files server addons, maps and everything ](http://www.brywright.co.uk/downloads/files/index.php?dir=natural-selection/)
* [Game manual (some outdated info)](https://www.unknownworlds.com/oldwebsite/manuals/comm_manual/basic/index.htm)
*  [rcbot downloads](http://filebase.bots-united.com/index.php?act=category&id=19)
*  [dillinger.io markdown editor](http://dillinger.io/)

   [Natural Selection]: <http://unknownworlds.com/ns/>
   [Unknownworlds Entertainment]:<https://github.com/unknownworlds/NS>
   [Steam]: <http://store.steampowered.com/about/>
   [hlds]:<https://developer.valvesoftware.com/wiki/Half-Life_Dedicated_Server>
   [rcbot]:<http://filebase.bots-united.com/index.php?act=category&id=19>


Half Life 1 SDK LICENSE
=======================

https://github.com/ValveSoftware/halflife/blob/master/README.md

You may, free of charge, download and use the SDK to develop a modified Valve game running on the Half-Life engine. You may distribute your modified Valve game in source and object code form, but only for free. Terms of use for Valve games are found in the Steam Subscriber Agreement located here: http://store.steampowered.com/subscriber_agreement/

You may copy, modify, and distribute the SDK and any modifications you make to the SDK in source and object code form, but only for free. Any distribution of this SDK must include this license.txt and third_party_licenses.txt.

DISCLAIMER OF WARRANTIES. THE SOURCE SDK AND ANY OTHER MATERIAL DOWNLOADED BY LICENSEE IS PROVIDED “AS IS”. VALVE AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES WITH RESPECT TO THE SDK, EITHER EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, TITLE AND FITNESS FOR A PARTICULAR PURPOSE.

LIMITATION OF LIABILITY. IN NO EVENT SHALL VALVE OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE THE ENGINE AND/OR THE SDK, EVEN IF VALVE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

Natural Selection copyright and trademarks
==========================================
All artwork, sounds, audio, screenshots, text and code in Natural Selection, Zen of Sudoku, Spark engine and Natural Selection 2 are Copyright © 2014 Unknown Worlds Entertainment, Inc (http://www.unknownworlds.com).

The mark Natural Selection was first represented in association with video-game software in June of 2001, and was first used in commerce around January, 2002. Natural Selection is Registered with the U.S. Patent and Trademark Office (No. 4,179,393).

Natural Selection license
=========================
See COPYING.txt for the GNU GENERAL PUBLIC LICENSE

EXCLUDED CODE: The code described below and contained in the Natural Selection Source Code release is not part of the Program covered by the GPL and is expressly excluded from its terms. You are solely responsible for obtaining from the copyright holder a license for such code and complying with the applicable license terms.

EXCLUDED CODE AND LIBRARIES
- Half Life 1 SDK LICENSE (Copyright(C) Valve Corp.)
- FMOD 3.7.5
- Lua 5.0 (http://lua.org)
- Particle system library by David McAllister (http://www.cs.unc.edu/techreports/00-007.pdf).

Original code and design by Charlie Cleveland (charlie@unknownworlds.com, @flayra).

Many contributions from Karl Patrick (karl.patrick@gmail.com), Petter Rønningen <tankefugl@gmail.com>, Harry Walsh <harry.walsh@gmail.com>, and probably lots of people I forgot.

