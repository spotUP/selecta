/*
 * Library skeleton for every UP- MUI library (up-mcc.library, UP-<Class>.mcc, UP-MCC.mcp, UP-Spike.mcc). It replaces libnix
 * libinit.o, whose Resident carries rt_Version 0 with no way to set it. exec's ramlib refuses to load a disk library whose
 * Resident version is below the version the caller asks for, so OpenLibrary("up-mcc.library", 1) from each class's
 * __UserLibInit failed, every class init failed, and MUI_NewObject returned NULL for all eleven classes (I685p).
 *
 * The code is libinit.o transcribed (same register use, same base layout as libnix libinit.h struct libBase, same calls into
 * __UserLibInit / __UserLibCleanup), written as one file-scope asm block so the order is fixed: the -1 returning entry point
 * first, then the vectors, the InitTable and the Resident. Only rt_Version differs: it is UP_LIB_VERSION, the same value the
 * header file gives LibVersion (lib_Version after init), set once per target by CMake (MCC_LIB_VERSION). This file must be the
 * first source on the link line. cmake/mcc_check_romtag.cmake checks every built file after linking.
 *
 * Function table: .list___FuncTable__ starts with one placeholder long (InitTable points 4 bytes past it), then Open, Close,
 * Expunge, Reserved; each header file appends its own vectors with ADDTABL_* and ends the list with ADDTABL_END.
 */
#ifndef UP_LIB_VERSION
#error "UP_LIB_VERSION missing: CMake sets it from MCC_LIB_VERSION"
#endif
#if UP_LIB_VERSION < 1 || UP_LIB_VERSION > 255
#error "UP_LIB_VERSION must fit rt_Version (1..255)"
#endif

#define UP_STR2(x) #x
#define UP_STR(x) UP_STR2(x)

__asm__(
    "\t.text\n"
    "\t.even\n"
    "\t.globl _safefail\n"
    "_safefail:\n"
    "\tmoveq #-1,d0\n"
    "\trts\n"

    "\t.globl _LibExtFunc\n"
    "_LibExtFunc:\n"
    "\tmoveq #0,d0\n"
    "\trts\n"

    /* Expunge (a6 = base): mark delayed expunge; refuse while open. */
    "\t.globl _LibExpunge\n"
    "_LibExpunge:\n"
    "\torib #8,a6@(14)\n"
    "\ttstw a6@(32)\n"
    "\tjeq 1f\n"
    "\tmoveq #0,d0\n"
    "\trts\n"
    "1:\n"
    "\tjbsr _up_lib_expunge\n"
    "\trts\n"

    /* Close: drop the open count; expunge when it reaches 0 with a delayed expunge pending. */
    "\t.globl _LibClose\n"
    "_LibClose:\n"
    "\tmovew a6@(32),d0\n"
    "\tsubqw #1,d0\n"
    "\tmovew d0,a6@(32)\n"
    "\tjne 2f\n"
    "\tmoveb a6@(14),d0\n"
    "\tbtst #3,d0\n"
    "\tjne 3f\n"
    "2:\n"
    "\tmoveq #0,d0\n"
    "\trts\n"
    "3:\n"
    "\tjbsr _up_lib_expunge\n"
    "\trts\n"

    "\t.globl _LibOpen\n"
    "_LibOpen:\n"
    "\tandib #-9,a6@(14)\n"
    "\taddqw #1,a6@(32)\n"
    "\tmovel a6,d0\n"
    "\trts\n"

    /* Init (d0 = base, a0 = seglist, a6 = SysBase): fill the node, call __UserLibInit(base); non-zero frees the base. */
    "\t.globl _LibInit\n"
    "_LibInit:\n"
    "\tmoveml a2/a4/a6,sp@-\n"
    "\tmoveal d0,a2\n"
    "\tmoveb #9,a2@(8)\n"
    "\tmovel #_LibName,a2@(10)\n"
    "\tmoveb #6,a2@(14)\n"
    "\tmovew _LibVersion,a2@(20)\n"
    "\tmovew _LibRevision,a2@(22)\n"
    "\tmovel #_LibIdString,a2@(24)\n"
    "\tmovel a0,a2@(36)\n"
    "\tmovel a6,a2@(44)\n"
    "\tmovel d0,sp@-\n"
    "\tmoveal a2@(40),a4\n"
    "\tjsr ___UserLibInit\n"
    "\taddql #4,sp\n"
    "\ttstl d0\n"
    "\tjne 4f\n"
    "\tmovel a2,d0\n"
    "\tmoveml sp@+,a2/a4/a6\n"
    "\trts\n"
    "4:\n"
    "\tmoveq #0,d1\n"
    "\tmovew a2@(18),d1\n"
    "\tmoveq #0,d0\n"
    "\tmovew a2@(16),d0\n"
    "\tmoveal a2,a1\n"
    "\tsubal d0,a1\n"
    "\taddl d1,d0\n"
    "\tjsr a6@(-210)\n"
    "\tsubal a0,a0\n"
    "\tmovel a0,d0\n"
    "\tmoveml sp@+,a2/a4/a6\n"
    "\trts\n"

    /* InitTable: base size 0x30 (struct libBase), function table, no data init, LibInit. */
    "\t.even\n"
    "_up_InitTab:\n"
    "\t.long 0x30\n"
    "\t.long ___FuncTable__+4\n"
    "\t.long 0\n"
    "\t.long _LibInit\n"

    /* Resident: RTC_MATCHWORD, self pointer, end skip, RTF_AUTOINIT, version, NT_LIBRARY, pri 0, name, id, InitTable. */
    "\t.globl _up_RomTag\n"
    "_up_RomTag:\n"
    "\t.word 0x4afc\n"
    "\t.long _up_RomTag\n"
    "\t.long _up_RomTagEnd\n"
    "\t.byte 0x80\n"
    "\t.byte " UP_STR(UP_LIB_VERSION) "\n"
    "\t.byte 9\n"
    "\t.byte 0\n"
    "\t.long _LibName\n"
    "\t.long _LibIdString\n"
    "\t.long _up_InitTab\n"
    "_up_RomTagEnd:\n"

    /* Expunge body (a6 = base): Remove, __UserLibCleanup, free the base, return the seglist. */
    "_up_lib_expunge:\n"
    "\tmoveml d2/a2/a4/a6,sp@-\n"
    "\tmoveal a6,a2\n"
    "\tmoveal a2,a1\n"
    "\tmoveal a6@(44),a6\n"
    "\tjsr a6@(-252)\n"
    "\tmoveal a2@(40),a4\n"
    "\tjsr ___UserLibCleanup\n"
    "\tmovel a2@(36),d2\n"
    "\tmoveq #0,d1\n"
    "\tmovew a2@(18),d1\n"
    "\tmoveq #0,d0\n"
    "\tmovew a2@(16),d0\n"
    "\tmoveal a2,a1\n"
    "\tsubal d0,a1\n"
    "\taddl d1,d0\n"
    "\tjsr a6@(-210)\n"
    "\tmovel d2,d0\n"
    "\tmoveml sp@+,d2/a2/a4/a6\n"
    "\trts\n"

    "\t.section .list___FuncTable__,\"aw\"\n"
    "\t.even\n"
    "\t.globl ___FuncTable__\n"
    "___FuncTable__:\n"
    "\t.long 0\n"
    "\t.long _LibOpen\n"
    "\t.long _LibClose\n"
    "\t.long _LibExpunge\n"
    "\t.long _LibExtFunc\n"
    "\t.text\n");
