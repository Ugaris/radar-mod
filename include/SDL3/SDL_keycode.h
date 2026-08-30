/*
 * Minimal SDL_Keycode shim for building mods without the SDL3 development
 * headers. The client passes SDL3 keycodes as 32-bit values; this typedef
 * is ABI-identical to SDL3's (typedef Uint32 SDL_Keycode). If you build
 * against the real SDL3 headers instead, delete this file.
 */
#ifndef SDL_keycode_h_
#define SDL_keycode_h_

typedef unsigned int SDL_Keycode;

#endif /* SDL_keycode_h_ */
