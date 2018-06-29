/* BERMUDA SYNDROME WII V0.4, "coder" : wiimpathy */

Instructions 
------------

- Extract the content of the archive to your SD card or your USB disc.
- You need the original data files of the game.
Copy the BERMUDA folder content of your game in sd:/bermuda/data or usb:/bermuda/data.
Content : 01, 02, 03 ... jack and so on ...
- If you've lost your game, you can find it on abandonware websites(I can't give direct links).

HOW TO PLAY
-----------

- WIIMOTE :

D-PAD = MOVE
A     = ACTION
1     = RUN / TAKE AWAY WEAPON	
2     = USE WEAPON
-     = SHOW STATUS / HEALTH
+     = INVENTORY
HOME  = IN-GAME MENU


- CLASSIC CONTROLLER :

D-PAD = MOVE
A     = ACTION
B     = RUN / TAKE AWAY WEAPON
Y     = USE WEAPON
X     = INVENTORY
L     = SHOW STATUS / HEALTH
HOME  = IN-GAME MENU


- GAMECUBE PAD :

D-PAD = MOVE
A     = ACTION
B     = RUN / TAKE AWAY WEAPON
Y     = USE WEAPON
X     = INVENTORY
Z     = SHOW STATUS / HEALTH
START = IN-GAME MENU


CHANGELOG
---------

- v0.4 
* Recompiled with devkitPPC r31
* Standard Wii Makefile
* Use libvorbis library from source. Link with libogg from devkitPPC.
* Some cleanups.

- v0.3 
* Recompiled with devkitPPC r26

- v0.2 
* Better usb support(ios58).
* Use BMP instead of PNG for background menu(libpng issue with latest SDL-Wii). 

- v0.1 
* initial release.

CREDITS/THANKS
--------------

- Century Interactive : the original game.
- Gregory Montoir : the game engine reimplementation.
- Libogc/devkitppc dreamteam
- L.
- Mother
