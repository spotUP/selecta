/* DUI-P0-2: the smallest translation unit that needs SDL3 headers and the library, so the dependency is built
 * and linked by the plain build. Replaced by backend_sdl.c in DUI-P2-7. */
#include <SDL3/SDL.h>

int sel_desktop_sdl_version(void)
{
    return SDL_GetVersion();
}
