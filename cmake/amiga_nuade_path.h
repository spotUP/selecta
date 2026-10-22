/* Forced include for third_party/nuade/src/nuade_path.c on the 68k build.
 * libnix <string.h> declares a global concat(); nuade_path.c defines a static
 * function of that name with another signature. Include the header first,
 * then rename nuade's function. Our build file, no change to nuade. */
#include <string.h>
#define concat nuade_path_concat
