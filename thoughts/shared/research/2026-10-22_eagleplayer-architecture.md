---
date: 2026-10-22
topic: Eagleplayer 2.04 plugin architecture (research)
source: /Users/spot/Downloads/Eagleplayer2.04 (read-only, grep/sed only, nothing executed)
---

# Eagleplayer 2.04 architecture

## Summary

- Eagleplayer is a 68000 assembler, AmigaOS-only player. Readme.txt (about lines 51-53) says porting is "senseless and impossible" and advises writing a new player. There is NO OS-independence claim.
- Plugins are Amiga executables or segments carrying an 8-byte ID plus a tag array: players use `'EPPLAYER'`, engines use `'EPEngine'`. Tag families cross the boundary: DTP_*, EP_*, MI_*, EPPI_*, EUT_*, EPAMT_*, CV_*, FL_*, RPT_*, SS_*.
- Engines come in two models: a task engine (own process, messages to port `EAGLEPLAYERPORT`) and a tag engine (EP calls handlers listed in EUS_TagList). GUIs are tag engines of type EUTY_MainWindow.
- Replayers write to audio in one of two ways: Direct (Paula-style registers through the ENPP_Poke* jump table) or Amplifier (filled `AS_` audio structs that a software amplifier mixes into its own buffer). The 8 Bit and 14 Bit amplifiers still touch `$dff0a0` / `$dff096`, so they drive Paula themselves.
- Best seams to copy for a new player: the tag-array plugin header, the `AS_` struct plus amplifier jump table, the DTN_NoteStruct (notes to mixer), module-info and pattern-info tags, and the staged Check1..8 format detection.
- Unverified: internals of the amplifier sources beyond grep hits, EagleAMPGui.s, EMPYGui24.s, and the bodies of the four small plugin includes.

## 1. Player plugin interface

- Include/misc/DeliPlayer.i (471 lines, DeliTracker-derived):
  - L36 `STRUCTURE DeliTrackerPlayer`: dtp_RTS_code, dtp_ID (8 bytes), dtp_TagArray. DTP_TagBase = TAG_USER+"DT".
  - Tags: identity (RequestDTVersion, PlayerVersion, PlayerName, Creator), Check1 (first bytes, before load) and Check2 (after full load, dtg_ChkSize), ExtLoad, Interrupt, Stop, Config/UserConfig, SubSongRange/NewSubSongRange/SubSongTest, InitPlayer/EndPlayer/InitSound/EndSound/StartInt/StopInt, Volume, Balance, Faster, Slower, NextPatt, PrevPatt, NextSong, PrevSong, Decrunch, Convert, NotePlayer/NoteStruct, Process.
  - ~L161 PLAYERHEADER macro: `bra.w start`, 8-byte ID, pointer to tag array. Check returns d0=0 when the format is OK. Interrupt runs at 50 Hz. A5 holds globals.
  - L251-300 DTN_NoteStruct / DTN_NoteChannel: notes-to-mixer abstraction (sample start/length, repeat, frequency, volume, stereo pos). NSTF_* sample-format flags (period, signed/unsigned, ulaw/alaw, float, 7..64 bit).
  - L326 `DeliTrackerGlobals` (dtg_*): library bases, ChkData/ChkSize, SndNum/SndVol/balance, and function pointers (LoadFile, AudioAlloc/AudioFree, StartInt/StopInt, SongEnd, SetTimer, WaitAudioDMA, NotePlayer, ...).
- Include/misc/EaglePlayer.i (802 lines, release 2.02):
  - EAGLEVERSION=13 (L24). EP_TagBase=TAG_USER+17744: Get_ModuleInfo, Voices, SampleInit/End, Save, StructInit/End, Load/SavePlConfig, GetPositionNr, SetSpeed, Check3..8, CheckSegment, InitAmplifier (~L78), SetPosition (L91), and more.
  - EPF_*/EPB_* capability flags (~L96-150).
  - MI_TagBase=TAG_USER+19785: module info (SongName, AuthorName, SubSongs, Length, Samples, Duration, PlayFrequency, Bitrate, Year, Genre, ...).
  - L227 `EP_Sampletable`; L275 `EP_Patterninfo`; L308 `UPS_USER` per-voice struct (Adr, Len, Per, Vol, Note, ...), written by the replayer and read by scopes/analyzers.
  - L390 EPR_* error codes 1-69; EPRS_* check errors.
  - L486 `EaglePlayerGlobals` (EPG_*); L666 NEWEP_GLOBALS: negative-offset table of about 60 six-byte JMP slots (AllocAudio L684, Poke interface L716-720, GetHardwareType L746, ...).
  - L753-754 EPHT_Amiga=0, EPHT_Draco=1. L782 EPPHEADER macro (`'EPPLAYER'`, taglist).
- Include/misc/EaglePlayerEngine.i L57: EPPI_TagBase=TAG_USER+20553 (player info and ratings).
- Detection order, Src/EP_Main.s L36-63: DTP_Check1, EP_Check3/4/6 run on the first 2200 bytes; EP_Check5 and DTP_Check2 on the full file. LoadFast changes chip/fast RAM placement. Call sites: ModuleGiveCheck L10089 (EP_CheckModule); load/check code L15486-16200 (Check7/8 are formatloader checks); EP_CheckSegment ~L15106; DTP_SubSongRange L9873/14532/14674; DTP_StartInt L9917.
- Loading: players are LoadSeg-ed (EP_Main.s ~L16783-16801); the tag array is found by the `EPPLAYER` ID.
- Decrunching: Src/EP_Decrunch.s: DecrunchPack L212, PPDecrunch L327, LHDecrunch L692, StoneDecrunch L788, CRM_Decrunch L949 (uses EPG_CrMBase). External libs (xpk, lh, powerpacker, xfdmaster, CrM) come via EPG_*Base. Format loaders: Engines/Formatloader.s, Playerloader.s.

## 2. Engines interface (EaglePlayerEngine.i, 2473 lines)

- Registration: `Engine_START` (EUS_*) at L114 at file start: bra.w, ID "EPEngine" (EUSN_Identifier L738), EUS_EPBase (set by EP), EUS_TagList, tick fields, MsgFlags, name/creator/version, saved prefs, EUS_Type, EUS_Flags.
- Types EUTY_*: Manager, SampleInfo, EngineInfo, PlayerInfo, Decruncher, Ripper, MainWindow (GUI), ScreenManager, DirViewer, Analyzer, Amplifier, PatternScroll, MessageWindow, ListAdministration, AmplifierManager, PlayerLoader, Extractor, Scope, ModuleInfo, Converter, FormatLoader, SampleSaver.
- Task engine (ExampleEngine.s, ExampleScope.s): own process. Header L33-77 documents hooks (INITSCOPE, OPENWINDOW returning a signal mask, SIGNALRECEIVED, CLOSEWINDOW, ENDSCOPE; optional NEWMODULE, KILLMODULE, STARTPLAY, STOPPLAY, INTERRUPT). EUS struct L117-161 (EUTY_Scope L156). Finds public port "EAGLEPLAYERPORT", sends USClass_NewEngine (~L525-560), then loops Wait/GetMsg/dispatch/ReplyMsg (~L740-775). Scopes read UPS data after STARTPLAY (ExampleScope.s ~L564-620).
- Tag engine (GUIs): EP calls EUT_* handlers (EaglePlayerEngine.i ~L310-720, EUT_TagBase=TAG_USER+21843). Groups: lifecycle (InitEngine, ExitEngine, Activate, DeActivate, Show, Hide), config, window (Open/CloseAWindow, GetWindowHandle, Iconify, Zip), event loop (SignalReceived, GetWaitMask, GetInterruptMask, GetMsg, Interrupt), menu/prefs (ChangeGui, ChangePrefs, GetGuiMenuStrip), notifications (NewModule, NewSong, StartInt, StopInt, KillModule, NewPlayer, ...), ripper/converter/archive, progress display (InitDisplay, FillDisplay, RemoveDisplay).
- Messages: `UM_Message` L777; types USM_* L803-807; classes USClass_* 0-37 L810-860; USMF_* MsgFlags (~L290-335) select which broadcasts an engine gets; UCM_* commands from engine to EP (~L880); EUSM_* FreeTable (~L748-775) lets EP clean up after a crashed engine.
- GUI switching: EP_Main.s ~L32770-32800 deactivates the old GUI (A5DeactivateAUser), sets UMG_UPrgSAdr, stores tags in UMG_Tags, calls EUT_Activate via A5CallUp3. L10875-10905: non-GUI tag engines get Activate right after load; MainWindow engines are skipped.
- What a GUI must implement (from the three examples): EUS header with EUTY_MainWindow, plus tags InitEngine, ExitEngine, OpenAWindow, CloseAWindow, DeActivate, GetMsg, GetWaitMask/SignalReceived, Enable/DisableAll, Get/Save/NewConfig, GetWindowHandle, progress display tags.
- GUI comparison:
  - BifatGUI.s: type L383, tags ~L402-440; full set including scroller tags (NewScrollText, PrintText, ClearTextWindow, Interrupt), GetAbortMsg, ChangeGui/ChangePrefs/GetPrefs, menu strip.
  - Eagleplayer1x-GUI.s: type L449, same set (~L466-493).
  - EMPYGui.s: type L266, tags ~L297-335. Smaller set: GetMsg=HandleWindowEvents, Interrupt, NewModule/KillModule/NewSong, StartInt/StopInt call UpdateUPS; SignalReceived and ChangeGui commented out; no scroller tags. Uses guigfx and true-colour images (EmpyTrue/*.0rgb). Sets AttnFlags=AFF_68020.
  - EagleAMPGui.s and EMPYGui24.s: not examined.

## 3. Amplifier interface

- EaglePlayerEngine.i L209 `Amplifier_Jumps` (AMJ_*): CheckFeatures, Init, StartInt, StopInt, End, Amplifier (receives filled AS_ structs), PokeAdr/Len/Per/Vol, DMAMask, Command (AMCMD_Filter). Poke entries only for amplifiers that poke hardware, else 0.
- Amplifier engine: EUS_Type EUTY_Amplifier with EUS_AMIDNr, priority, flags. AmplifierManager is EUTY_AmplifierManager (15).
- L2230-2292 `AS_` struct, one per voice: CurrentAdr/Pos/FPos, SampleSize, Period (Amiga values), RepeatAdr/Size, Left/RightVolume 0..64, DMABit, LeftRight (panning), LoopFlag, Changeflags, Int softint, private/reserved fields. Loop modes: None, Forwards, PingPong (Backwards unsupported).
- EPAMT_TagBase=TAG_USER+16717 (NumStructs, AudioStructs, MinEPVersion, Flags). EPAMF_*: Direct, 8Bit, 8BitUnsigned, 16Bit, ChipRam, PingPongLoops, WaitForStruct, AudioInts.
- A replayer opts in via EP_InitAmplifier (EaglePlayer.i ~L78). Globals: EPG_AmplifierList, ActiveAmplifier, AudioStruct. Internal chip/fast-RAM amplifiers live in Src/EP_InternalAmplifiers.s (1237 lines).
- Answer: both paths exist. Direct mode pokes Paula-style registers (ENPP_Poke*, DMAMask). Amplifier mode hands `AS_` structs to a software mixer. Grep hits: 8 Bit Amplifier.s (type L161, `$dff0a0` at L3260/3366/3415, DMA write `$800f` to `$dff096` L3417; AllocMem L1695/2836/2843) and 14 Bit Amplifier.s (type L174, EPAMF_8BitUnsigned/16Bit checks L2841-2945, `$dff0a0` L3704/3903/3956) mix into allocated buffers and then play via Paula channels (14 bit by combining channels, unverified in detail). Toccata-Amplifier.s (type L201, includes Toccata.i/Toccata_lib.i L51-52, uses Toccata_base L148, starts/stops playback via toccata.library L385/L432) outputs to a sound card, not Paula. Unverified: exact mixing code and rates.

## 4. Cross-boundary tags/structs and OS dependence

- Tag families: DTP_*, EP_*, MI_*, EPPI_*, EUT_*, EPAMT_*, CV_* (TAG_USER+"EC", EaglePlayerConverter.i L35), FL_* (+"FL", Formatloader.i L35), RPT_* (+"ER", Ripper.i L34), SS_* (+"SS", Samplesaver.i L39).
- Structs: PlayerHeader, DeliTrackerGlobals, EaglePlayerGlobals + NEWEP_GLOBALS jump table, EP_Sampletable, EP_Patterninfo, UPS_USER, DTN_NoteStruct/NoteChannel, Engine_START (EUS), UM_Message, AS_, Amplifier_Jumps, EPPl_ Playerstruct. Plus EPR_/EPRS_ errors and flag sets EPF/EPB, EPAMF, USMF, EUSF, EUIF, NSTF.
- AmigaOS-specific: exec ports/tasks/signals/AllocMem/LoadSeg, intuition/graphics/gadtools windows, ASL, locale, AmigaGuide, commodities, Rexx, CIA timers, Paula (period values, DMA bits, chip RAM), 68k register ABI (A5 = globals). Only EPHT_Draco hints at hardware abstraction.
- Readme: GPL from 2.04 (L25 onward) but external Eagleplayers (replayers) are not GPL (L66). Build: ASM-Pro 1.15, each plugin a standalone executable in EP:Engines/... (L124-131).

## 5. Developer docs in Include/misc

- BuggsMacros.i (19): macros.
- DeliPlayer.i (471): player call protocol, best commented.
- EaglePlayer.i (802): EP extensions, globals, errors, amplifier hook.
- EaglePlayerEngine.i (2473): engine interface, messages, AS_ struct; German design notes ~L2380-2473.
- EaglePlayerConverter.i (80) + .h (62): converter plugin tags.
- EaglePlayerFormatloader.i (84): formatloader plugin tags.
- EaglePlayerRipper.i (119): ripper plugin tags.
- EaglePlayerSamplesaver.i (90): sample saver plugin tags.
- No separate manual. Docs are include comments, ExampleEngine.s / ExampleScope.s headers, EP_Main.s header comments, and Readme.txt.
