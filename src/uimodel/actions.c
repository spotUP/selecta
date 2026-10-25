/* Action table of the desktop UI (plan DUI-P1-6; M4-P2-3). One row per thing a menu item, toolbar button or
 * shortcut can do: label in full English words, tooltip, icon id (lucide names), menu path, default key (the
 * keybind.c key name) and the protocol line it sends. Every COMMANDS verb has a row or an exemption below. */
#include <selecta/protocol.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

static const sel_uim_action k_actions[] = {
    {"play", "Play", "Start or resume playback", "play", "Playback/Play", "C-Enter", "PLAY"},
    {"pause", "Pause", "Pause playback", "pause", "Playback/Pause", "Space", "PAUSE"},
    {"stop", "Stop", "Stop playback", "square", "Playback/Stop", "s", "STOP"},
    {"next", "Next Track", "Skip to the next track", "skip-forward", "Playback/Next Track", "n", "NEXT"},
    {"prev", "Previous Track", "Return to the previous track", "skip-back", "Playback/Previous Track", "p", "PREV"},
    {"random_on", "Turn Shuffle On", "Play the queue in random order", "shuffle", "Playback/Turn Shuffle On", "",
     "OPTION random on"},
    {"random_off", "Turn Shuffle Off", "Play the queue in order", "shuffle", "Playback/Turn Shuffle Off", "",
     "OPTION random off"},
    {"repeat_off", "Turn Repeat Off", "Stop repeating", "repeat", "Playback/Turn Repeat Off", "", "REPEAT off"},
    {"repeat_all", "Repeat Queue", "Repeat the whole queue", "repeat", "Playback/Repeat Queue", "", "REPEAT all"},
    {"repeat_one", "Repeat Song", "Repeat the current song", "repeat", "Playback/Repeat Song", "", "REPEAT one"},
    {"subsong_prev", "Previous Subsong", "Go to the previous subsong", "minus", "Playback/Previous Subsong", "",
     "SUBSONG"},
    {"subsong_next", "Next Subsong", "Go to the next subsong", "plus", "Playback/Next Subsong", "", "SUBSONG"},
    {"next_album", "Next Album", "Skip to the next album", "chevrons-right", "Playback/Next Album", "M-n", "NEXT_ALBUM"},
    {"prev_album", "Previous Album", "Return to the previous album", "chevrons-left", "Playback/Previous Album", "M-p",
     "PREV_ALBUM"},
    {"seek_forward", "Seek Forward", "Jump five seconds ahead", "fast-forward", "Playback/Seek Forward", "M-Right",
     "SEEK +5000"},
    {"seek_back", "Seek Backward", "Jump five seconds back", "rewind", "Playback/Seek Backward", "M-Left", "SEEK -5000"},
    {"volume_up", "Volume Up", "Raise the volume by five", "volume-2", "Playback/Volume Up", "+", "VOLUME +5"},
    {"volume_down", "Volume Down", "Lower the volume by five", "volume-1", "Playback/Volume Down", "-", "VOLUME -5"},
    {"pitch_up", "Pitch Up", "Raise the pitch by one semitone", "arrow-up", "Playback/Pitch Up", ".", "PITCH +100"},
    {"pitch_down", "Pitch Down", "Lower the pitch by one semitone", "arrow-down", "Playback/Pitch Down", ",",
     "PITCH -100"},
    {"tempo_up", "Tempo Up", "Raise the tempo by five percent", "gauge", "Playback/Tempo Up", "C-Right", "TEMPO +5"},
    {"tempo_down", "Tempo Down", "Lower the tempo by five percent", "gauge", "Playback/Tempo Down", "C-Left",
     "TEMPO -5"},
    {"solo_clear", "Unsolo All Voices", "Play every voice again", "volume-x", "Playback/Unsolo All Voices", "0",
     "SOLO 0"},
    {"filter_led", "Toggle Filter Light", "Switch the Amiga filter LED on or off", "lightbulb", "Playback/Filter LED",
     "M-l", "FILTER_LED toggle"},
    {"queue_clear", "Clear Queue", "Remove every song from the queue", "trash-2", "Queue/Clear Queue", "M-Backspace",
     "QUEUE_CLEAR"},
    {"queue_shuffle", "Shuffle Queue", "Shuffle the queue", "shuffle", "Queue/Shuffle Queue", "M-s", "QUEUE_SHUFFLE"},
    {"queue_reshuffle", "Reshuffle Queue", "Shuffle the queue again", "refresh-cw", "Queue/Reshuffle Queue", "M-r",
     "RESHUFFLE"},
    {"undo", "Undo", "Undo the last queue change", "undo-2", "Edit/Undo", "u", "UNDO"},
    {"redo", "Redo", "Redo the undone queue change", "redo-2", "Edit/Redo", "C-r", "REDO"},
    {"like", "Like Song", "Like the playing song, or take the like back when it is liked (a song key names another song)", "heart", "Library/Like Song", "M-h", "LIKE"},
    {"cursor_up", "Cursor Up", "Move the cursor up one row", "chevron-up", "View/Cursor Up", "k", "VIEW_CURSOR -1"},
    {"cursor_down", "Cursor Down", "Move the cursor down one row", "chevron-down", "View/Cursor Down", "j",
     "VIEW_CURSOR +1"},
    {"cursor_top", "Cursor to Top", "Move the cursor to the first row", "arrow-up-to-line", "View/Cursor to Top",
     "Home", "VIEW_CURSOR top"},
    {"cursor_bottom", "Cursor to Bottom", "Move the cursor to the last row", "arrow-down-to-line",
     "View/Cursor to Bottom", "End", "VIEW_CURSOR bottom"},
    {"activate", "Open Selection", "Open or play the row under the cursor", "corner-down-left", "View/Open Selection",
     "Enter", "VIEW_ACTIVATE"},
    {"stats_show", "Show Statistics", "Show the statistics screen", "bar-chart-3", "View/Show Statistics", "b",
     "STATS_SHOW"},
    {"stats_hide", "Hide Statistics", "Return from the statistics screen", "x", "View/Hide Statistics", "C-b",
     "STATS_HIDE"},
    {"layout_full", "Full Layout", "Show the whole player window", "layout-dashboard", "View/Full Layout", "C-1",
     "PREF_SET ui.desktop.mode full"},
    {"layout_window", "Window Layout", "Show the player as a small window", "app-window", "View/Window Layout", "C-2",
     "PREF_SET ui.desktop.mode window"},
    {"layout_mini", "Mini Layout", "Show only the mini player", "minimize-2", "View/Mini Layout", "C-3",
     "PREF_SET ui.desktop.mode mini"},
    {"layout_visual", "Visual Layout", "Show the visualizer full screen with the signal path", "maximize", "View/Visual Layout",
     "C-4", "PREF_SET ui.desktop.mode visual"},
    {"viz_scope", "Oscilloscope Visualizer", "Draw the song as an oscilloscope trace", "activity",
     "View/Visualizer/Oscilloscope", "C-5", "PREF_SET ui.desktop.viz viz_scope"},
    {"viz_levels", "Levels Visualizer", "Draw one level bar per voice", "bar-chart", "View/Visualizer/Levels", "C-6",
     "PREF_SET ui.desktop.viz viz_levels"},
    {"viz_spectrum", "Spectrum Visualizer", "Draw the song as frequency bands", "audio-lines",
     "View/Visualizer/Spectrum", "C-7", "PREF_SET ui.desktop.viz viz_spectrum"},
    {"ui_text", "Switch to Text Interface", "Hand the session to the text interface", "terminal",
     "View/Switch to Text Interface", "C-t", "UI_SWITCH text"},
    {"ui_desktop", "Switch to Desktop Interface", "Hand the session to the desktop interface", "monitor",
     "View/Switch to Desktop Interface", "C-d", "UI_SWITCH desktop"},
};

/* Verbs that no menu item, button or shortcut sends as they stand. Each carries the reason. */
#define ARG "needs a typed argument from a dialog"
#define PROTO "protocol plumbing, never user visible"
#define CTX "reached through a context row, not a menu item"
#define COV "covered by the rows with a fixed argument"
static const struct {
    const char* verb;
    const char* reason;
} k_exempt[] = {
    {"OPEN", ARG},           {"PAUSE_PLAYBACK", COV}, 
              {"MUTE", CTX},                         
               {"OUTPUT", ARG},
    {"SLEEP_TIMER", ARG},    {"ALARM", ARG},          {"QUEUE_ADD", ARG},       {"QUEUE_ADD_QUERY", ARG},
    {"QUEUE_REMOVE", CTX},   {"QUEUE_PLAY_NEXT", CTX}, {"QUEUE_MOVE", CTX},     {"QUEUE_SWAP", CTX},
    {"QUEUE_CHANGES", PROTO}, {"SNAPSHOT_QUEUE", PROTO}, {"PLAY_QUERY", ARG},    {"SHUFFLE", ARG},
    {"SCOPE", ARG},          {"PLAYLIST_LOAD", ARG},  {"PLAYLIST_SAVE", ARG},   {"SAMPLE_LIST", CTX},
    {"COMPOSERS_SHOW", CTX}, {"COMPOSERS_HIDE", CTX},
    {"SAMPLE_EXPORT", CTX},  {"SAMPLE_PLAY", CTX},  {"PLAYLIST_PUBLISH", ARG}, {"LIBRARY_BROWSE", ARG}, {"LIBRARY_SEARCH", ARG},
    {"LIBRARY_FETCH", ARG},  {"LIBRARY_SCAN", ARG},   {"LIBRARY_SYNC", ARG},    {"SOURCES", PROTO},    {"KEEP", CTX},
    {"FLAG", CTX},           {"NOTE", ARG},           {"RATE", ARG},            {"VOTE", CTX},
    {"ANALYSE", CTX},        {"CUE_SET", CTX},        {"CUE_GOTO", CTX},        {"SHARE", ARG},
    {"RADIO", ARG},          {"STICKER_SET", PROTO},  {"STICKER_GET", PROTO},   {"STICKER_DEL", PROTO},
    {"FOLDER_ADD", ARG},     {"FOLDER_MOVE", CTX},    {"FOLDER_DEL", CTX},      {"PLAYLIST_MOVE", CTX},
    {"PLAYLIST_CREATE", ARG}, {"PLAYLIST_ADD_SONG", CTX}, {"PLAYLIST_RENAME", ARG}, {"PLAYLIST_DEL", CTX}, {"STICKER_LIST", PROTO}, {"PREF_GET", PROTO},     
    {"VIEW_STYLE", CTX},     {"VIEW_SHOW", CTX},      {"VIEW_SAVE", ARG},       {"VIEW_SOURCE", CTX},
    {"VIEW_WINDOW", PROTO},  {"VIEW_GROUP", CTX},     {"VIEW_EXPAND", CTX},     {"VIEW_COLLAPSE", CTX},
    {"VIEW_DRILL", CTX},     {"MIX_STEREO_WIDTH", ARG}, {"DSP", ARG},           {"BEGIN", PROTO},
    {"END", PROTO},          {"COMMANDS", PROTO},     {"EVENTS", PROTO},        {"TAGTYPES", PROTO},
    {"IDLE", PROTO},         {"AUTH", PROTO},         {"DEVICE_LIST", PROTO},   {"DEVICE_TRANSFER", ARG},
    {"DEVICE_FOLLOW", ARG},
};
#undef ARG
#undef PROTO
#undef CTX
#undef COV

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

uint32_t sel_uim_action_count(void) { return (uint32_t)COUNT(k_actions); }

const sel_uim_action* sel_uim_action_at(uint32_t i) { return i < COUNT(k_actions) ? &k_actions[i] : NULL; }

uint32_t sel_uim_exemption_count(void) { return (uint32_t)COUNT(k_exempt); }

const char* sel_uim_exemption_at(uint32_t i, const char** reason) {
    if (i >= COUNT(k_exempt)) {
        return NULL;
    }
    if (reason != NULL) {
        *reason = k_exempt[i].reason;
    }
    return k_exempt[i].verb;
}

void sel_uim_help_row(uint32_t i, char* out, size_t cap) {
    const sel_uim_action* a = sel_uim_action_at(i);
    if (cap == 0u) {
        return;
    }
    if (a == NULL) {
        out[0] = '\0';
        return;
    }
    snprintf(out, cap, "%s  %s", a->menu, a->key[0] != '\0' ? a->key : "-");
}

static int pref_known(const char* key, size_t n) {
#define SEL_PREF(id, k, def, kind)                     \
    if (strlen(k) == n && strncmp(k, key, n) == 0) {   \
        return 1;                                      \
    }
#include <selecta/prefs.def>
#undef SEL_PREF
    return 0;
}

/* Every protocol line starts with a COMMANDS verb, or is PREF_SET of a prefs.def key; every verb has a row or one
 * exemption, never both. Returns 0 and an empty message when the table is sound. */
int sel_uim_actions_check(char* msg, size_t cap) {
    uint32_t i, v;
    msg[0] = '\0';
    for (i = 0; i < COUNT(k_actions); i++) {
        const char* line = k_actions[i].line;
        size_t n = strcspn(line, " ");
        char verb[40];
        uint32_t id;
        if (n >= sizeof verb) {
            snprintf(msg, cap, "line too long: %s", line);
            return 1;
        }
        memcpy(verb, line, n);
        verb[n] = '\0';
        if (!sel_cmd_by_name(verb, &id)) {
            snprintf(msg, cap, "action %s: %s is not a COMMANDS verb", k_actions[i].id, verb);
            return 1;
        }
        if (strcmp(verb, "PREF_SET") == 0) {
            const char* key = line + n + 1;
            if (!pref_known(key, strcspn(key, " "))) {
                snprintf(msg, cap, "action %s: unknown pref in %s", k_actions[i].id, line);
                return 1;
            }
        }
    }
    for (v = 0; v < SEL_CMD_COUNT; v++) {
        const char* name = sel_cmd_name(v);
        uint32_t rows = 0, ex = 0;
        for (i = 0; i < COUNT(k_actions); i++) {
            size_t n = strcspn(k_actions[i].line, " ");
            rows += strlen(name) == n && strncmp(name, k_actions[i].line, n) == 0;
        }
        for (i = 0; i < COUNT(k_exempt); i++) {
            ex += strcmp(k_exempt[i].verb, name) == 0;
        }
        if (rows == 0u ? ex != 1u : ex != 0u) {
            snprintf(msg, cap, "verb %s has %u rows and %u exemptions", name, (unsigned)rows, (unsigned)ex);
            return 1;
        }
    }
    return 0;
}
