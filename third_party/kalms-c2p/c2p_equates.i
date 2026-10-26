; c2p_equates.i  --  Up Rough Demo System
; ----------------------------------------------------------------------------
; PORT support for the vendored Kalms C2P collection.
;
; A handful of the vendored Kalms .s files `include` AmigaOS NDK headers
; (hardware/custom.i, lvo/exec_lib.i, lvo/graphics_lib.i, graphics/gfx.i).
; The project ships only a partial NDK (docs/ndk/hardware-i/CUSTOM.I), so
; those `include` lines are PORT-redirected to this file (see RULE 9: vasm
; syntax/availability fixups are permitted, tagged `*--- PORT`).
;
; This file provides ONLY the few stable symbols those routines actually
; reference. The numeric values are the canonical AmigaOS NDK 3.1 values:
;   - exec.library / graphics.library LVO offsets
;   - struct BitMap field offsets (graphics/gfx.i)
; Nothing here is project-specific; these are the same constants any NDK
; header would supply.
; ----------------------------------------------------------------------------

	IFND	C2P_EQUATES_I
C2P_EQUATES_I	SET	1

; --- exec.library LVOs (lvo/exec_lib.i) ---
; Used by the cpu-only 030 routines for instruction-cache flush after SMC.
	IFND	_LVOCacheClearU
_LVOCacheClearU	EQU	-636
	ENDC

; --- graphics.library LVOs (lvo/graphics_lib.i) ---
; Used by the cpu+blitter (c3b1 / c4b1) routines.
	IFND	_LVOQBlit
_LVOQBlit	EQU	-276
	ENDC

; --- struct BitMap (graphics/gfx.i) ---
; Used by the bitmap/ routines that take a BitMap* in a1.
	IFND	bm_BytesPerRow
bm_BytesPerRow	EQU	0
	ENDC
	IFND	bm_Rows
bm_Rows		EQU	2
	ENDC
	IFND	bm_Flags
bm_Flags	EQU	4
	ENDC
	IFND	bm_Depth
bm_Depth	EQU	5
	ENDC
	IFND	bm_pad
bm_pad		EQU	6
	ENDC
	IFND	bm_Planes
bm_Planes	EQU	8
	ENDC

	ENDC	; C2P_EQUATES_I
