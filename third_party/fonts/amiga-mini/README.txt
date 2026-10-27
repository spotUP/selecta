Amiga bitmap fonts mini4, mini6, mini8 (disk fonts: mini4/6, mini4/8, mini6/6, mini6/8, mini8/8 and the
.font contents files).

Origin: /Volumes/home/fonts on Johan's machine, copied 2026-10-27. Old public domain Amiga fonts, shipped as
they are (Johan, 2026-10-27). The author is not recorded in the files.

Format: AmigaOS loadable font, HUNK_HEADER 0x3F3 + one HUNK_CODE holding a DiskFontHeader and a TextFont.
tools/gui_fontbake.c reads them through plugins/ui/desktop/amiga_font.c at build time and bakes them into
sel_font_baked (family mini4, mini6, mini8, size = tf_YSize). The build never reads /Volumes/home.
The .font contents files are kept for provenance; the baker does not read them.
