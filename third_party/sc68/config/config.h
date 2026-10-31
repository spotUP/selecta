/* Hand-written replacement for sc68's autotools config.h (Selecta build, see PATCHES.txt). */
#define PACKAGE_NAME "sc68"
#define PACKAGE_VERSION "3.0.0a4"
#define PACKAGE_STRING "sc68 3.0.0a4"
#define PACKAGE_URL "http://sc68.atari.org"
#define PACKAGE_VERNUM 30000
#define HAVE_STDINT_H 1
#define HAVE_ASSERT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_GETENV 1
#define HAVE_FSYNC 1
#define HAVE_LIBGEN_H 1
#define HAVE_BASENAME 1

/* Upstream configure passes this as a define; the ICE! depacker (unice68) is always built here. */
#define FILE68_UNICE68 1
