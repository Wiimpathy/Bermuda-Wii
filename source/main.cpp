/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#include "game.h"
#include "systemstub.h"
#include <unistd.h>
#ifdef GEKKO
#include <gccore.h>
#include <ogcsys.h>
#include <dirent.h>
#include "fat.h"
#include <sys/stat.h>

// SDL framebuffers
extern GXRModeObj *vmode;
extern unsigned int *xfb[2];

extern "C" void __exception_setreload(int t);
#endif


static SystemStub *g_stub;

static const char *USAGE =
	"Bermuda Syndrome\n"
	"Usage: bs [OPTIONS]...\n"
	"  --datapath=PATH   Path to data files (default 'DATA')\n"
	"  --savepath=PATH   Path to save files (default '.')\n"
	"  --musicpath=PATH  Path to music files (default 'MUSIC')\n";

static bool parseOption(const char *arg, const char *longCmd, const char **opt) {
	bool handled = false;
	if (arg[0] == '-' && arg[1] == '-') {
		if (strncmp(arg + 2, longCmd, strlen(longCmd)) == 0) {
			*opt = arg + 2 + strlen(longCmd);
			handled = true;
		}
	}
	return handled;
}

static void exitMain() {
	if (g_stub) {
		g_stub->destroy();
		delete g_stub;
		g_stub = 0;
	}
}

#ifdef GEKKO
bool DirExist(const char *path)
{
	DIR *dir;
	dir = opendir(path);
	if(dir)
	{
		closedir(dir);
		return true;
	}
	return false;
}
#endif


#undef main
//int main(int argc, char *argv[]) {
extern "C" int SDL_main(int argc, char **argv) {

#ifdef GEKKO
	__exception_setreload(8);

	fatInitDefault();
	bool isUSB = false;

	/* Check if game's data exist : USB first then SD */
	if(DirExist("usb:/bermuda/data/jack/"))
	{
		isUSB=true;
	}
	else if(DirExist("sd:/bermuda/data/jack/"))
	{
		isUSB=false;
	}
	else
	{
		console_init(xfb[0],20,20,vmode->fbWidth,vmode->xfbHeight,vmode->fbWidth*VI_DISPLAY_PIX_SZ);
		printf("\nNo game data found !\n");
		printf("Check your sd:/bermuda or usb:/bermuda folder.\n");
		printf("Exiting ...");
		sleep(5);
		exit(0);
	}

	const char *dataPath= (isUSB) ? "usb:/bermuda/data" : "sd:/bermuda/data";
	const char *savePath= (isUSB) ? "usb:/bermuda/save" : "sd:/bermuda/save";
	const char *musicPath= (isUSB) ? "usb:/bermuda/music" : "sd:/bermuda/music";
#else
	const char *dataPath = "DATA";
	const char *savePath = ".";
	const char *musicPath = "MUSIC";
#endif

	for (int i = 1; i < argc; ++i) {
		bool opt = false;
		if (strlen(argv[i]) >= 2) {
			opt |= parseOption(argv[i], "datapath=", &dataPath);
			opt |= parseOption(argv[i], "savepath=", &savePath);
			opt |= parseOption(argv[i], "musicpath=", &musicPath);
		}
		if (!opt) {
			printf("%s", USAGE);
			return 0;
		}
	}
	g_debugMask = DBG_INFO; // | DBG_GAME | DBG_OPCODES | DBG_DIALOGUE;
	g_stub = SystemStub_SDL_create();
	atexit(exitMain);
	Game *g = new Game(g_stub, dataPath, savePath, musicPath);
	g->mainLoop();
	delete g;
	fatUnmount("sd:");
	fatUnmount("usb:");
	return 0;
}
