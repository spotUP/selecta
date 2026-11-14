/* Startup for an Amiga on-demand plugin module (I427e, plugin-loader-sdk D8; design addendum
 * thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md). Linked FIRST into every module hunk file with
 * -nostartfiles, so this code sits at offset 0 of the first hunk. It replaces libnix's ncrt0.o, which may not be
 * reused: ncrt0 waits for a Workbench message when pr_CLI is 0 and calls main().
 *
 * Layout at offset 0 of the first code hunk (the host reads it at BADDR(seg) + 4):
 *   +0  moveq #-1,d0 ; rts      a module run from a shell exits with failure
 *   +4  'SELP'                  magic
 *   +8  2                       header version (1: the I427e header, no imports; the loader still takes it)
 *   +12 init                    int init(void): sets SysBase, runs libnix's INIT_LIST (opens libraries, runs C++
 *                               constructors); 1 = ready, 0 = an init function called exit()
 *   +16 fini                    void fini(void): runs EXIT_LIST (frees the malloc pool, closes libraries)
 *   +20 sel_plugin_entry        the plugin's normal entry; host API through the sel_host table only
 *   +24 _sel_seg_imp            v2 (I1378a): writable slot the loader fills with the host's sel_seg_imports table
 *                               before init; the generated thunks (cmake/amiga_seg_imports.cmake) jump through it
 *   +28 SEL_SEG_IMPORT_COUNT    v2: length of cmake/amiga_seg_imports.txt this module was built from (0 = no imports);
 *                               the loader rejects the module when it exceeds the host's table
 *
 * exit() inside init or fini unwinds to the saved stack pointer and returns 0. callfuncs is libnix's own
 * priority walk, copied from ncrt0.o (pairs of function, priority; NULL ends the list). */

#ifndef SEL_SEG_IMPORT_COUNT
#define SEL_SEG_IMPORT_COUNT 0
#endif
#define SEL_SEG_STR_(x) #x
#define SEL_SEG_STR(x) SEL_SEG_STR_(x)

__asm__(
    "	.text\n"
    "	.globl	_sel_seg_start\n"
    "_sel_seg_start:\n"
    "	moveq	#-1,d0\n"
    "	rts\n"
    "	.long	0x53454c50\n"
    "	.long	2\n"
    "	.long	_sel_seg_init\n"
    "	.long	_sel_seg_fini\n"
    "	.long	_sel_plugin_entry\n"
    "	.globl	_sel_seg_imp\n"
    "_sel_seg_imp:\n"
    "	.long	0\n"
    "	.long	" SEL_SEG_STR(SEL_SEG_IMPORT_COUNT) "\n"
    "_sel_seg_init:\n"
    "	moveml	d2-d7/a2-a6,sp@-\n"
    "	movel	4:w,_SysBase\n"
    "	clrl	sel_seg_ok\n"
    "	bsrs	1f\n"
    "	movel	sel_seg_ok,d0\n"
    "	moveml	sp@+,d2-d7/a2-a6\n"
    "	rts\n"
    "1:	movel	sp,___SaveSP\n"
    "	lea	___INIT_LIST__+4,a2\n"
    "	moveq	#-1,d2\n"
    "	bsrs	sel_seg_callfuncs\n"
    "	moveq	#1,d0\n"
    "	movel	d0,sel_seg_ok\n"
    "	rts\n"
    "_sel_seg_fini:\n"
    "	moveml	d2-d7/a2-a6,sp@-\n"
    "	bsrs	2f\n"
    "	moveml	sp@+,d2-d7/a2-a6\n"
    "	rts\n"
    "2:	movel	sp,___SaveSP\n"
    "	lea	___EXIT_LIST__+4,a2\n"
    "	moveq	#0,d2\n"
    "	bsrs	sel_seg_callfuncs\n"
    "	rts\n"
    "	.globl	_exit\n"
    "	.globl	__exit\n"
    "	.globl	____exit\n"
    "	.globl	__Exit\n"
    "_exit:\n"
    "__exit:\n"
    "____exit:\n"
    "__Exit:\n"
    "	movel	___SaveSP,sp\n"
    "	lea	___EXIT_LIST__+4,a2\n"
    "	moveq	#0,d2\n"
    "	bsrs	sel_seg_callfuncs\n"
    "	moveq	#0,d0\n"
    "	rts\n"
    "sel_seg_callfuncs:\n"
    "	lea	sel_seg_pri,a5\n"
    "	moveal	a2,a3\n"
    "	moveq	#0,d3\n"
    "	bras	4f\n"
    "3:	movel	a3@+,d4\n"
    "	movel	a5@,d5\n"
    "	cmpl	d4,d5\n"
    "	bnes	5f\n"
    "	moveal	d0,a0\n"
    "	jsr	a0@\n"
    "5:	eorl	d2,d4\n"
    "	eorl	d2,d5\n"
    "	cmpl	d5,d4\n"
    "	bccs	4f\n"
    "	cmpl	d3,d4\n"
    "	blss	4f\n"
    "	movel	d4,d3\n"
    "4:	movel	a3@+,d0\n"
    "	bnes	3b\n"
    "	eorl	d2,d3\n"
    "	movel	d3,a5@\n"
    "	cmpl	d2,d3\n"
    "	bnes	sel_seg_callfuncs\n"
    "	rts\n"
    "	.globl	___main\n"
    "___main:\n"
    "	rts\n"
    "	.data\n"
    "	.globl	_SysBase\n"
    "	.globl	___SaveSP\n"
    "_SysBase:	.long	0\n"
    "___SaveSP:	.long	0\n"
    "sel_seg_ok:	.long	0\n"
    "sel_seg_pri:	.long	0\n"
    "sel_seg_refs:	.long	___initlibraries\n"
    "	.long	___initcpp\n"
    "	.long	___nocommandline\n"
    "	.section .dlist___LIB_LIST__,\"aw\"\n"
    "	.globl	___LIB_LIST__\n___LIB_LIST__:	.long	0\n"
    "	.section .list___INIT_LIST__,\"aw\"\n"
    "	.globl	___INIT_LIST__\n___INIT_LIST__:	.long	0\n"
    "	.section .list___EXIT_LIST__,\"aw\"\n"
    "	.globl	___EXIT_LIST__\n___EXIT_LIST__:	.long	0\n"
    "	.section .list___CTOR_LIST__,\"aw\"\n"
    "	.globl	___CTOR_LIST__\n___CTOR_LIST__:	.long	0\n"
    "	.section .list___DTOR_LIST__,\"aw\"\n"
    "	.globl	___DTOR_LIST__\n___DTOR_LIST__:	.long	0\n"
    "	.section .end_of_lists,\"aw\"\n"
    "	.long	0\n"
    "	.section .end_of_dlists,\"aw\"\n"
    "	.long	0\n"
    "	.comm	___commandline,4\n"
    "	.comm	___commandlen,4\n"
    "	.comm	__WBenchMsg,4\n"
    "	.comm	___env,4\n"
    "	.comm	___argv,4\n"
    "	.comm	___argc,4\n"
    "	.text\n");
