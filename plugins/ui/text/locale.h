/* Character set of the terminal from the POSIX locale variables (row I218d). UP-Term on AmigaOS
 * decodes program output per personality: Latin-1 for the amiga personality, UTF-8 for xterm, and it
 * tells programs with LC_CTYPE=C.UTF-8. Pure C99: the caller passes the variable values, so a host
 * test drives it without an environment. */
#ifndef SELECTA_TEXT_LOCALE_H
#define SELECTA_TEXT_LOCALE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 when the locale names UTF-8. The first set, non-empty value of LC_ALL, LC_CTYPE, LANG decides
 * (POSIX precedence); it names UTF-8 when it contains "UTF-8" or "UTF8" in any case. NULL means unset. */
int txt_locale_utf8(const char* lc_all, const char* lc_ctype, const char* lang);

#ifdef __cplusplus
}
#endif

#endif
