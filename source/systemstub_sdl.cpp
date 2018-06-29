/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#include <SDL/SDL.h>
#include "systemstub.h"

#ifdef GEKKO
#include <gccore.h>
#include <ogcsys.h>
#include <di/di.h>
#include <fat.h>

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>

#define VERSION "v0.4"
#endif

int slotwii=1; 

static SDL_Joystick *sdlJoystick[6] =   /* SDL's joystick structures */
{
	NULL, NULL, NULL, NULL, NULL, NULL
};

enum {
	kMaxBlitRects = 50,
	kSoundSampleRate = 22050,
	kSoundSampleSize = 4096,
	kVideoSurfaceDepth = 32
};


struct SystemStub_SDL : SystemStub {
	uint32 *_offscreen;
	uint32 *_offscreenPrev;
	int _offscreenSize;
	bool _blurOn;
	SDL_Surface *_screen;
	SDL_PixelFormat *_fmt;
	uint32 _pal[256];
	int _screenW, _screenH;
	SDL_Rect _blitRects[kMaxBlitRects];
	int _blitRectsCount;
	bool _fullScreenRedraw;
	bool _fullScreenDisplay;
	int _soundSampleRate;
	SDL_Overlay *_yuv;
	bool _yuvLocked;

	SystemStub_SDL()
		: _offscreen(0), _offscreenPrev(0), _screen(0),
		_yuv(0), _yuvLocked(false) {
	}
	virtual ~SystemStub_SDL() {}

	virtual void init(const char *title, int w, int h);
	virtual void destroy();
	virtual void setPalette(const uint8 *pal, int n);
	virtual void fillRect(int x, int y, int w, int h, uint8 color);
	virtual void copyRect(int x, int y, int w, int h, const uint8 *buf, int pitch, bool transparent);
	virtual void darkenRect(int x, int y, int w, int h);
	virtual void updateScreen();
	virtual void setYUV(bool flag, int w, int h);
	virtual uint8 *lockYUV(int *pitch);
	virtual void unlockYUV();
	virtual void processEvents();
	virtual void sleep(int duration);
	virtual uint32 getTimeStamp();
	virtual void lockAudio();
	virtual void unlockAudio();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual int getOutputSampleRate();

	void clipDirtyRect(int &x, int &y, int &w, int &h);
	void addDirtyRect(int x, int y, int w, int h);
	void setScreenDisplay(bool fullscreen);
	void MenuOptions(SDL_Surface* screen, int slot);
};

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

void SystemStub_SDL::init(const char *title, int w, int h) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
	TTF_Init();
	int i, nPadsConnected;
	/* Scan the joysticks */
	nPadsConnected = SDL_NumJoysticks();
	for (i = 0; i < nPadsConnected && i < 6; i++)
	{
		/* Open the joysticks for use */
		sdlJoystick[i] = SDL_JoystickOpen(i);
		/* joystick connected? */
		if (sdlJoystick[i] != NULL)		
		debug(DBG_INFO, "Joystick %d connected!",SDL_JoystickName(i));		
	}
//
#ifdef GEKKO
	SDL_ShowCursor(SDL_DISABLE);
#else
	SDL_ShowCursor(SDL_ENABLE);
#endif
	SDL_WM_SetCaption(title, NULL);
	_quit = false;
	memset(&_pi, 0, sizeof(_pi));
	_screenW = w;
	_screenH = h;
	_offscreenSize = w * h * sizeof(uint32);
	_offscreen = (uint32 *)malloc(_offscreenSize);
	if (!_offscreen) {
		error("SystemStub_SDL::init() Unable to allocate offscreen buffer");
	}
	memset(_offscreen, 0, _offscreenSize);
#ifdef BERMUDA_BLUR
	_offscreenPrev = (uint32 *)malloc(_offscreenSize);
	if (_offscreenPrev) {
		memset(_offscreenPrev, 0, _offscreenSize);
	}
#endif
	_blurOn = false;
	memset(_pal, 0, sizeof(_pal));
	_fullScreenDisplay = false;
	setScreenDisplay(_fullScreenDisplay);
	_soundSampleRate = 0;
}

void SystemStub_SDL::destroy() {
	if (_offscreenPrev) {
		free(_offscreenPrev);
		_offscreenPrev = 0;
	}
	if (_offscreen) {
		free(_offscreen);
		_offscreen = 0;
	}
	if (_screen) {
		// free()'d by SDL_Quit()
		_screen = 0;
	}
	SDL_Quit();
}

void SystemStub_SDL::setPalette(const uint8 *pal, int n) {
	assert(n <= 256);
	_pal[0] = SDL_MapRGB(_fmt, 0, 0, 0); pal += 4;
	for (int i = 1; i < n; ++i) {
		_pal[i] = SDL_MapRGB(_fmt, pal[2], pal[1], pal[0]);
		pal += 4;
	}
	_fullScreenRedraw = true;
}

void SystemStub_SDL::fillRect(int x, int y, int w, int h, uint8 color) {
	clipDirtyRect(x, y, w, h);
	if (w <= 0 || h <= 0) {
		return;
	}
	addDirtyRect(x, y, w, h);
	const uint32 fillColor = _pal[color];
	uint32 *p = _offscreen + y * _screenW + x;
	while (h--) {
		for (int i = 0; i < w; ++i) {
			p[i] = fillColor;
		}
		p += _screenW;
	}
}

void SystemStub_SDL::copyRect(int x, int y, int w, int h, const uint8 *buf, int pitch, bool transparent) {
	if (_blitRectsCount >= kMaxBlitRects) {
		warning("SystemStub_SDL::copyRect() Too many blit rects, you may experience graphical glitches");
		return;
	}
	clipDirtyRect(x, y, w, h);
	if (w <= 0 || h <= 0) {
		return;
	}
	addDirtyRect(x, y, w, h);

	uint32 *p = _offscreen + y * _screenW + x;
	buf += h * pitch;
	while (h--) {
		buf -= pitch;
		for (int i = 0; i < w; ++i) {
			if (!transparent || buf[i] != 0) {
				p[i] = _pal[buf[i]];
			}
		}
		p += _screenW;
	}
}

void SystemStub_SDL::darkenRect(int x, int y, int w, int h) {
	if (_blitRectsCount >= kMaxBlitRects) {
		warning("SystemStub_SDL::darkenRect() Too many blit rects, you may experience graphical glitches");
		return;
	}
	clipDirtyRect(x, y, w, h);
	if (w <= 0 || h <= 0) {
		return;
	}
	addDirtyRect(x, y, w, h);

	const uint32 redBlueMask = _fmt->Rmask | _fmt->Bmask;
	const uint32 greenMask = _fmt->Gmask;

	uint32 *p = _offscreen + y * _screenW + x;
	while (h--) {
		for (int i = 0; i < w; ++i) {
			uint32 color = ((p[i] & redBlueMask) >> 1) & redBlueMask;
			color |= ((p[i] & greenMask) >> 1) & greenMask;
			p[i] = color;
		}
		p += _screenW;
	}
}

static uint32 blurPixel(int x, int y, const uint32 *src, int pitch, int w, int h, SDL_PixelFormat *fmt) {
	static const int blurMat[3 * 3] = {
		1, 2, 1,
		2, 4, 2,
		1, 2, 1
	};
	static const int blurMatSigma = 16;

	const uint32 redBlueMask = fmt->Rmask | fmt->Bmask;
	const uint32 greenMask = fmt->Gmask;

	uint32 redBlueBlurSum = 0;
	uint32 greenBlurSum = 0;

	for (int v = 0; v < 3; ++v) {
		int ym = y + v - 1;
		if (ym < 0) {
			ym = 0;
		} else if (ym >= h) {
			ym = h - 1;
		}
		for (int u = 0; u < 3; ++u) {
			int xm = x + u - 1;
			if (xm < 0) {
				xm = 0;
			} else if (xm >= w) {
				xm = w - 1;
			}
			assert(ym >= 0 && ym < h);
			assert(xm >= 0 && xm < w);
			const uint32 color = src[ym * pitch + xm];
			const int mul = blurMat[v * 3 + u];
			redBlueBlurSum += (color & redBlueMask) * mul;
			greenBlurSum += (color & greenMask) * mul;
		}
	}
	return ((redBlueBlurSum / blurMatSigma) & redBlueMask) | ((greenBlurSum / blurMatSigma) & greenMask);
}

void SystemStub_SDL::updateScreen() {
	if (_fullScreenRedraw) {
		_fullScreenRedraw = false;
		_blitRectsCount = 1;
		SDL_Rect *br = &_blitRects[0];
		br->x = 0;
		br->y = 0;
		br->w = _screenW;
		br->h = _screenH;
	}
	SDL_LockSurface(_screen);
	if (_blurOn) {
		uint32 *dst;
		const uint32 *src, *srcPrev;
		for (int i = 0; i < _blitRectsCount; ++i) {
			SDL_Rect *br = &_blitRects[i];
			for (int y = br->y; y < br->y + br->h; ++y) {
				dst = (uint32 *)_screen->pixels + y * _screen->pitch / sizeof(uint32);
				src = _offscreen + y * _screenW;
				srcPrev = _offscreenPrev + y * _screenW;
				for (int x = br->x; x < br->x + br->w; ++x) {
					if (srcPrev[x] != src[x]) {
						dst[x] = blurPixel(x, y, _offscreen, _screenW, _screenW, _screenH, _fmt);
					} else {
						dst[x] = src[x];
					}
				}
			}
		}
		memcpy(_offscreenPrev, _offscreen, _offscreenSize);
	} else {
		for (int i = 0; i < _blitRectsCount; ++i) {
			SDL_Rect *br = &_blitRects[i];
			uint8 *dst = (uint8 *)_screen->pixels + br->y * _screen->pitch + br->x * 4;
			const uint32 *src = _offscreen + br->y * _screenW + br->x;
			for (int h = 0; h < br->h; ++h) {
				memcpy(dst, src, br->w * 4);
				dst += _screen->pitch;
				src += _screenW;
			}
		}
	}
	SDL_UnlockSurface(_screen);
	SDL_UpdateRects(_screen, _blitRectsCount, _blitRects);
	_blitRectsCount = 0;
}

void SystemStub_SDL::setYUV(bool flag, int w, int h) {
	if (flag) {
		if (!_yuv) {
			_yuv = SDL_CreateYUVOverlay(w, h, SDL_UYVY_OVERLAY, _screen);
		}
	} else {
		if (_yuv) {
			SDL_FreeYUVOverlay(_yuv);
			_yuv = 0;
		}
	}
	_yuvLocked = false;
}

uint8 *SystemStub_SDL::lockYUV(int *pitch) {
	if (_yuv && !_yuvLocked) {
		if (SDL_LockYUVOverlay(_yuv) == 0) {
			_yuvLocked = true;
			if (pitch) {
				*pitch = _yuv->pitches[0];
			}
			return _yuv->pixels[0];
		}
	}
	return 0;
}

void SystemStub_SDL::unlockYUV() {
	if (_yuv && _yuvLocked) {
		SDL_UnlockYUVOverlay(_yuv);
		_yuvLocked = false;
		SDL_Rect r;
		if (_yuv->w * 2 <= _screenW && _yuv->h * 2 <= _screenH) {
			r.w = _yuv->w * 2;
			r.h = _yuv->h * 2;
		} else {
			r.w = _yuv->w;
			r.h = _yuv->h;
		}
		r.x = (_screenW - r.w) / 2;
		r.y = (_screenH - r.h) / 2;
		SDL_DisplayYUVOverlay(_yuv, &r);
	}
}

void SystemStub_SDL::processEvents() {
	bool paused = false;


	while (1) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
			case SDL_QUIT:
				_quit = true;
				break;
			case SDL_ACTIVEEVENT:
				if (ev.active.state & SDL_APPINPUTFOCUS) {
					paused = ev.active.gain == 0;
					SDL_PauseAudio(paused ? 1 : 0);
				}
				break;

#ifdef GEKKO
			case SDL_JOYHATMOTION:
			{
				if ( ev.jhat.value & SDL_HAT_LEFT )
				_pi.dirMask=4;
				else if ( ev.jhat.value & SDL_HAT_RIGHT )
				_pi.dirMask=8;
				else if ( ev.jhat.value & SDL_HAT_UP )
				_pi.dirMask=1;
				else if ( ev.jhat.value & SDL_HAT_DOWN )
				_pi.dirMask=2;
				else
				_pi.dirMask=0;
			}
			break;

			case SDL_JOYBUTTONUP:
			{
			switch (ev.jbutton.which) {
				/* Wiimote 1 Classic Controller 1 */
				case 0:
					if(ev.jbutton.button==0)			/* Wiimote 1 Button A */
						_pi.enter = false;	
					else  if(ev.jbutton.button==1)		/* Wiimote 1 Button B */
						_pi.escape = false;	
					else  if(ev.jbutton.button==2)		/* Wiimote 1 Button 1 */
						_pi.shift = false;	
					else  if(ev.jbutton.button==3)		/* Wiimote 1 Button 2 */
						_pi.space = false;	
					else  if(ev.jbutton.button==4)		/* Wiimote 1 Button - */
						_pi.ctrl = false;
					else  if(ev.jbutton.button==5)		/* Wiimote 1 Button + */
						_pi.tab = false;
					else if(ev.jbutton.button==9)		/* Classic 1 Button A */
						_pi.enter = false;	
					else  if(ev.jbutton.button==10)		/* Classic 1 Button B */
						_pi.shift = false;
					else  if(ev.jbutton.button==11)		/* Classic 1 Button X */
						_pi.tab = false;	
					else  if(ev.jbutton.button==12)		/* Classic 1 Button Y */
						_pi.space = false;	
					else  if(ev.jbutton.button==13)		/* Classic 1 Button L */
						_pi.ctrl = false;
					else  if(ev.jbutton.button==14)		/* Classic 1 Button R */
						_pi.escape = false;
				break;

				/* Gamecube Controller 1 */
				case 4:
					if(ev.jbutton.button==0)
						_pi.enter = false;	
					else  if(ev.jbutton.button==1)
						_pi.shift = false;	
					else  if(ev.jbutton.button==2)
						_pi.tab = false;	
					else  if(ev.jbutton.button==3)
						_pi.space = false;	
					else  if(ev.jbutton.button==4)
						_pi.ctrl = false;
				break;
				}
				break;

		}
		break;

		case SDL_JOYBUTTONDOWN:
		{
			switch (ev.jbutton.which) {
				/* Wiimote 1 Classic Controller 1 */
				case 0:				
					if(ev.jbutton.button==0)			/* Wiimote 1 Button A */
						_pi.enter = true;					
					else  if(ev.jbutton.button==1)		/* Wiimote 1 Button B */
						_pi.escape = true;
					else  if(ev.jbutton.button==2)		/* Wiimote 1 Button 1 */
						_pi.shift = true;					
					else  if(ev.jbutton.button==3)		/* Wiimote 1 Button 2 */					
						_pi.space = true;	
					else  if(ev.jbutton.button==4)		/* Wiimote 1 Button - */
						_pi.ctrl = true;					
					else  if(ev.jbutton.button==5)		/* Wiimote 1 Button + */
						_pi.tab = true;				
					else  if(ev.jbutton.button==6)  {	/* Wiimote 1 Button Home */				
						MenuOptions(_screen, slotwii);
						break;					
					}
					else if(ev.jbutton.button==9)		/* Classic 1 Button A */
						_pi.enter = true;	
					else  if(ev.jbutton.button==10)		/* Classic 1 Button B */
						_pi.shift = true;
					else  if(ev.jbutton.button==11)		/* Classic 1 Button X */
						_pi.tab = true;	
					else  if(ev.jbutton.button==12)		/* Classic 1 Button Y */
						_pi.space = true;	
					else  if(ev.jbutton.button==13)		/* Classic 1 Button L */
						_pi.ctrl = true;
					else  if(ev.jbutton.button==14)		/* Classic 1 Button R */
						_pi.escape = true;
					else  if(ev.jbutton.button==19)  {	/* Classic 1 Button Home */						
						MenuOptions(_screen, slotwii);
						break;
					}
				break;
				/* Gamecube Controller 1 */
				case 4:
					if(ev.jbutton.button==0)			/* Gamecube 1 Button A */
						_pi.enter = true;					
					else  if(ev.jbutton.button==1)		/* Gamecube 1 Button B */
						_pi.shift = true;
					else  if(ev.jbutton.button==2)		/* Gamecube 1 Button X */
						_pi.tab = true;					
					else  if(ev.jbutton.button==3)		/* Gamecube 1 Button Y */						
						_pi.space = true;	
					else  if(ev.jbutton.button==4)		/* Gamecube 1 Button Z */
						_pi.ctrl = true;								
					else  if(ev.jbutton.button==7)  {	/* Gamecube 1 Button L */				
						MenuOptions(_screen, slotwii);
						break;					
					}
				break;

			}
			break;
		}
		break;
#endif

			case SDL_KEYUP:
				switch (ev.key.keysym.sym) {
				case SDLK_LEFT:
					_pi.dirMask &= ~PlayerInput::DIR_LEFT;
					break;
				case SDLK_RIGHT:
					_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
					break;
				case SDLK_UP:
					_pi.dirMask &= ~PlayerInput::DIR_UP;
					break;
				case SDLK_DOWN:
					_pi.dirMask &= ~PlayerInput::DIR_DOWN;
					break;
				case SDLK_RETURN:
					_pi.enter = false;
					break;
				case SDLK_SPACE:
					_pi.space = false;
					break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
					_pi.shift = false;
					break;
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					_pi.ctrl = false;
					break;
				case SDLK_TAB:
					_pi.tab = false;
					break;
				case SDLK_ESCAPE:
					_pi.escape = false;
					break;
				default:
					break;
				}
				break;
			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym) {
				case SDLK_LEFT:
					_pi.dirMask |= PlayerInput::DIR_LEFT;
					break;
				case SDLK_RIGHT:
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
					break;
				case SDLK_UP:
					_pi.dirMask |= PlayerInput::DIR_UP;
					break;
				case SDLK_DOWN:
					_pi.dirMask |= PlayerInput::DIR_DOWN;
					break;
				case SDLK_RETURN:
					_pi.enter = true;
					break;
				case SDLK_SPACE:
					_pi.space = true;
					break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
					_pi.shift = true;
					break;
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					_pi.ctrl = true;
					break;
				case SDLK_TAB:
					_pi.tab = true;
					break;
				case SDLK_ESCAPE:
					_pi.escape = true;
					break;
				case SDLK_f:
					_pi.fastMode = !_pi.fastMode;
					break;
				case SDLK_s:
					_pi.save = true;
					break;
				case SDLK_l:
					_pi.load = true;
					break;
				case SDLK_b:
					if (_offscreenPrev) {
						_blurOn = !_blurOn;
						_fullScreenRedraw = true;
					}
					break;
				case SDLK_w:
					_fullScreenDisplay = !_fullScreenDisplay;
					setScreenDisplay(_fullScreenDisplay);
					break;
				case SDLK_KP_PLUS:
				case SDLK_PAGEUP:
					_pi.stateSlot = 1;
					break;
				case SDLK_KP_MINUS:
				case SDLK_PAGEDOWN:
					_pi.stateSlot = -1;
					break;
				default:
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (ev.button.button == SDL_BUTTON_LEFT) {
					_pi.leftMouseButton = true;
				} else if (ev.button.button == SDL_BUTTON_RIGHT) {
					_pi.rightMouseButton = true;
				}
				_pi.mouseX = ev.button.x;
				_pi.mouseY = ev.button.y;
				break;
			case SDL_MOUSEBUTTONUP:
				if (ev.button.button == SDL_BUTTON_LEFT) {
					_pi.leftMouseButton = false;
				} else if (ev.button.button == SDL_BUTTON_RIGHT) {
					_pi.rightMouseButton = false;
				}
				_pi.mouseX = ev.button.x;
				_pi.mouseY = ev.button.y;
				break;
			case SDL_MOUSEMOTION:
				_pi.mouseX = ev.motion.x;
				_pi.mouseY = ev.motion.y;
				break;
			default:
				break;
			}
		}
		if (paused) {
			SDL_Delay(100);
		} else {
			break;
		}
	}
}

void SystemStub_SDL::sleep(int duration) {
	SDL_Delay(duration);
}

uint32 SystemStub_SDL::getTimeStamp() {
	return SDL_GetTicks();
}

void SystemStub_SDL::lockAudio() {
	SDL_LockAudio();
}

void SystemStub_SDL::unlockAudio() {
	SDL_UnlockAudio();
}

void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired, obtained;
	memset(&desired, 0, sizeof(desired));
	desired.freq = kSoundSampleRate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = kSoundSampleSize;
	desired.callback = callback;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, &obtained) == 0) {
		_soundSampleRate = obtained.freq;
		SDL_PauseAudio(0);
	} else {
		error("SystemStub_SDL::startAudio() Unable to open sound device");
	}
}

void SystemStub_SDL::stopAudio() {
	SDL_CloseAudio();
}

int SystemStub_SDL::getOutputSampleRate() {
	return _soundSampleRate;
}

void SystemStub_SDL::clipDirtyRect(int &x, int &y, int &w, int &h) {
	if (x < 0) {
		x = 0;
	}
	if (x + w > _screenW) {
		w = _screenW - x;
	}
	if (y < 0) {
		y = 0;
	}
	if (y + h > _screenH) {
		h = _screenH - y;
	}
}

void SystemStub_SDL::addDirtyRect(int x, int y, int w, int h) {
	assert(_blitRectsCount < kMaxBlitRects);
	SDL_Rect *br = &_blitRects[_blitRectsCount];
	br->x = x;
	br->y = y;
	br->w = w;
	br->h = h;
	++_blitRectsCount;
}

void SystemStub_SDL::setScreenDisplay(bool fullscreen) {
	_screen = SDL_SetVideoMode(_screenW, _screenH, kVideoSurfaceDepth, fullscreen ? (SDL_HWSURFACE | SDL_FULLSCREEN) : SDL_HWSURFACE);
	if (!_screen) {
		error("SystemStub_SDL::init() Unable to allocate _screen buffer");
	}
	_fmt = _screen->format;
	memset(_blitRects, 0, sizeof(_blitRects));
	_blitRectsCount = 0;
	_fullScreenRedraw = true;
}

#ifdef GEKKO
void SystemStub_SDL::MenuOptions(SDL_Surface* screen, int slot)
 {
	#define SCREEN_WIDTH 640
	#define SCREEN_HEIGHT 480
	#define VERSION_Y 430

	int done = 0, i = 0, option = 1;
	Uint8 oldpad;
	char SlotTxt[]="1";
	sprintf(SlotTxt,"SLOT: %d",slot);

	/* Load fonts */
	TTF_Font *versionfont, *optionfont;
	versionfont = TTF_OpenFont("/apps/bermuda/font.ttf", 13);
	optionfont = TTF_OpenFont("/apps/bermuda/font.ttf", 24); 
	SDL_Color White = {0xFF, 0xFF, 0xFF} , Yellow = {255, 255, 0} , Green = {119, 170, 68};

// FIXME : wiimpathy test slotTXT with const 	 
	char const *menu[5] = {VERSION, "RESTORE GAME", "SAVE GAME", SlotTxt, "LEAVE BERMUDA"};
// char *menu[5] = {VERSION, "RESTORE GAME", "SAVE GAME", SlotTxt, "LEAVE BERMUDA"};
	SDL_Surface* s_menu[9], *background = NULL;
	/* Load BMP instead. Tired of libpng error with the latest SDL-Wii */			
 	//background = IMG_Load("/apps/bermuda/bg.png");
	background = SDL_LoadBMP("/apps/bermuda/bg.bmp");

	s_menu[0] = TTF_RenderText_Blended(versionfont, menu[0], White);

	/* Load text options */
    for (i = 1; i < 5; i++)
        s_menu[i] = TTF_RenderText_Blended(optionfont, menu[i], Green);
	
	s_menu[5] = TTF_RenderText_Blended(optionfont, "RESTORE GAME", Yellow);
	s_menu[6] = TTF_RenderText_Blended(optionfont, "SAVE GAME", Yellow);
	s_menu[7] = TTF_RenderText_Blended(optionfont, SlotTxt, Yellow);
	s_menu[8] = TTF_RenderText_Blended(optionfont, "LEAVE BERMUDA", Yellow);
	
	SDL_Rect version, positionBg;
	version.x = 550;
	version.y = VERSION_Y;
 	positionBg.x=0;
	positionBg.y=0;

    SDL_Rect pos_option; 
	SDL_Event event;

	while (!done)
	{		
		SDL_BlitSurface(background, NULL, screen, &positionBg);
		/* Wii version number*/
		SDL_BlitSurface(s_menu[0], NULL, screen, &version);		

		SDL_PollEvent(&event);

		switch(event.type)
		 {
		case SDL_JOYHATMOTION:
		{
			if ( (option > 1) && (event.jhat.value & SDL_HAT_UP) && !(oldpad))
            	option--;        
			else if ((option < 4) && (event.jhat.value & SDL_HAT_DOWN) && !(oldpad))
				option++;

			/* Change slot with left/right pad */
			if ((option == 3) && (event.jhat.value & SDL_HAT_LEFT) && !(oldpad))  {
				slot--;
				if (slot < 1 ) slot = 1;
				_pi.stateSlot--;
				sprintf(SlotTxt,"SLOT: %d",slot);
				s_menu[7] = TTF_RenderText_Blended(optionfont, SlotTxt, Yellow);
				s_menu[3] = TTF_RenderText_Blended(optionfont, SlotTxt, Green);
				break;
			}
			else if ((option == 3) && (event.jhat.value & SDL_HAT_RIGHT) && !(oldpad))	{
				_pi.stateSlot++;
				slot++;
				sprintf(SlotTxt,"SLOT: %d",slot);
				s_menu[7] = TTF_RenderText_Blended(optionfont, SlotTxt, Yellow);
				s_menu[3] = TTF_RenderText_Blended(optionfont, SlotTxt, Green);
				break;
			}
		}
		break;

		case SDL_JOYBUTTONDOWN:
			if(event.jbutton.button==0 || event.jbutton.button==9)
			{
				switch (option)
				{
				/* Save game */
				case 1:
					_pi.load = true;
					done = 1;
					break;
				/* Load game */
				case 2:
					_pi.save = true; 
					done = 1;
					break;
				/* Load/save slot */
				case 3:				
					break;
				/* Quit game */
				case 4:
					_quit=true;
					done = 1;
					break;
				}
            	break;  
            }
			if(event.jbutton.button==6 || event.jbutton.button==19)
			{				
				 done = 1;
				 break;
			}
		break;
	}
		/* Draw options */
		for (i = 1; i < 5; i++)
		{
			pos_option.x = (640 / 2) - (380 / 2);
			pos_option.y = 100 + 70 * (i - 1);
			version.x = 550;
			version.y = VERSION_Y;

			if (option == i)
			{
				boxRGBA(screen,
						pos_option.x - 6, pos_option.y - 12,
		 				pos_option.x + 380, pos_option.y + s_menu[option]->h + 12,
						32, 48, 64, 100);
						rectangleRGBA(screen,
		 				pos_option.x - 6, pos_option.y - 12,
		 				pos_option.x  + 380, pos_option.y + s_menu[option]->h + 12,
						140, 148, 16, 255);

				if(option==3)
				{
					trigonRGBA(screen,
								514, 228,
								514, 282,
								568, 255,
								140, 148, 16, 255);
					trigonRGBA(screen,
								120, 228,
								120, 282,
								66, 255,
								140, 148, 16, 255);
				}
				
				SDL_BlitSurface(s_menu[4+i], NULL, screen, &pos_option);
				SDL_BlitSurface(s_menu[0], NULL, screen, &version);				
			}
			else
			{
				boxRGBA(screen,
				pos_option.x - 6, pos_option.y - 12,
 				pos_option.x + 380, pos_option.y + s_menu[option]->h + 12,				
 				32, 48, 64, 100);

				rectangleRGBA(screen,
 				pos_option.x - 6, pos_option.y - 12,
 				pos_option.x  + 380, pos_option.y + s_menu[option]->h + 12,
				90, 99, 33, 255);
				SDL_BlitSurface(s_menu[i], NULL, screen, &pos_option);
				SDL_BlitSurface(s_menu[0], NULL, screen, &version);
			}
		} 
		SDL_Flip(screen);
 		oldpad = event.jhat.value;		
	}
 
	SDL_FreeSurface(screen);
	SDL_FreeSurface(background);

	for (i = 0; i < 9; i++)
		SDL_FreeSurface(s_menu[i]);
 
	TTF_CloseFont(versionfont);
	TTF_CloseFont(optionfont); 
}
#endif
