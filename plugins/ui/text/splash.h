/* Start screen art for the text UI (row I355). Hand-written from splash.txt, the source; keep the two in step.
 * 24 rows, at most 77 columns. Each byte is a code point (Latin-1 equals Unicode below 0x100), so the grid output
 * path renders it as Latin-1 on an Amiga shell and as UTF-8 on a host terminal. High bytes are \x escapes. */
#ifndef SELECTA_TEXT_SPLASH_H
#define SELECTA_TEXT_SPLASH_H

#define TUI_SPLASH_ROWS 24u
#define TUI_SPLASH_COLS 77u

static const char* const tui_splash[TUI_SPLASH_ROWS] = {
    "                                   _______ ",
    "                 __,             _/ _____/___  /         ,__",
    "   \xab-,_   _.----.)/__          \\ \\______    / /         __\\(.----._   _,-\xbb",
    "       )_//`----'\\\\_ l_,-         _/________\\       -,_l _//`----'\\\\_(",
    "       _\\\\       _//_            /   ____/_             _\\\\_       //_",
    "    -=(  ( ______ )  )=-        /   _/_\\   \\         -=(  ( ______ )  )=-",
    "       \\__((\xb0\\ /))__/          /____________\\           \\__((\\ /\xb0))__/",
    "     _ ___`\\__._/___ ___ _       /    /_____       _ ___ ___\\_.__/'___ _",
    "     \\))           ((__///      /    /_\\    \\ \\    \\\\\\__))           ((/",
    "       \\___,      ___/       / /_____________\\ \\       \\___      ,___/",
    "         \\ \\     ( /  \xa9Mt   /    /   ____/__        \xa9Mt  \\ )     / /",
    "          \\ \\     \\\\__.         /    _/_\\   \\         .__//     / /",
    "   ___ \xa1---\\_\\__   \\_----._    /_____________\\    _.----_/   __/_/---\xa1 ___",
    "-+-' `-'   (   ,)-\\-|      |     /    ____/_     |      |-/-(,   )   `-' `-+-",
    "       _.---\\___\\._ `------'    /    /      \\    `------' _./___/---._",
    "      //   (__) --.\\_ \\        /_____________\\        / _/.-- (__)   \\\\",
    "     / \\__________/(_) \\         /    /_______       / (_)\\__________/ \\",
    "    /___________________\\       /     __/    /      /___________________\\",
    "    `\\                 /'    / /_____________\\      `\\                 /'",
    "     /\\_______________/\\    /     _\\___   /          /\\_______________/\\",
    "    `-------------------'      / /    /    \\        `-------------------'",
    "                              / /___________\\ \\",
    "",
    "sPACE t0 sTART! - tHE bADDEST pLAYER t0 eVER pLAY tHE gAME! - sPACE t0 sTART!"
};

#endif
