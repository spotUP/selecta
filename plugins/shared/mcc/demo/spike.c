/* I685b spike program: opens the class UP-Spike.mcc BY NAME through MUI (MUI_NewObject finds MUI:Libs/mui/UP-Spike.mcc), puts it
 * in a window, quits after about 3 seconds (or at once with argument QUIT). The class code is not linked in.
 * Output: "spike: ok class=UP-Spike.mcc" (exit 0) or "spike: FAIL <reason>" (exit 20). */
#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dos.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

struct Library* MUIMasterBase;

static int fail(const char* why) {
    printf("spike: FAIL %s\n", why);
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
    }
    return 20;
}

int main(int argc, char** argv) {
    Object *app, *win, *spike;
    ULONG sigs = 0;
    ULONG is_open = 0;
    int quit_now = argc > 1 && argv[1] != NULL && strcmp(argv[1], "QUIT") == 0;
    int ticks = 0;

    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL) {
        return fail("muimaster.library 19");
    }
    spike = MUI_NewObject("UP-Spike.mcc", TAG_DONE);
    if (spike == NULL) {
        return fail("MUI_NewObject UP-Spike.mcc returned NULL");
    }
    win = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "UP-Spike.mcc", MUIA_Window_ID, 0x53504B31,
                        MUIA_Window_RootObject, (ULONG)spike, TAG_DONE);
    if (win == NULL) {
        MUI_DisposeObject(spike);
        return fail("window");
    }
    app = MUI_NewObject((char*)MUIC_Application, MUIA_Application_Title, (ULONG) "Spike", MUIA_Application_Base,
                        (ULONG) "SPIKE", MUIA_Application_Window, (ULONG)win, TAG_DONE);
    if (app == NULL) {
        MUI_DisposeObject(win);
        return fail("application");
    }
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &is_open);
    if (!is_open) {
        MUI_DisposeObject(app);
        return fail("window did not open");
    }
    while (!quit_now && ticks < 30) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
        if (id == (ULONG)MUIV_Application_ReturnID_Quit) {
            break;
        }
        Delay(5); /* 5/50 s per round, 30 rounds: about 3 s */
        ticks++;
    }
    MUI_DisposeObject(app);
    printf("spike: ok class=UP-Spike.mcc\n");
    CloseLibrary(MUIMasterBase);
    return 0;
}
