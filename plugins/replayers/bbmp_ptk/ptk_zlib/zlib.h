/* zlib API over the vendored miniz for the Amiga build (row I710qa): the Amiga toolchain has no libz. miniz.h maps the zlib names
 * (inflateInit, inflate, deflate, z_stream, ...) onto mz_* when MINIZ_NO_ZLIB_COMPATIBLE_NAMES is not set; selecta_archive defines it
 * publicly for its own sake, so it is cleared here. The mz_* symbols come from selecta_archive. */
#ifdef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "miniz.h"
