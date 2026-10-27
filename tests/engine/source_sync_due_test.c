/* I527: the stale decision behind the sync-on-launch banner. A remote source is offered for sync when it was never
 * synced, or the later of its last sync and its last "Not now" is `days` days old. Days 0 turns the offer off. */
#include <selecta/source.h>
#include <stdio.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

int main(void) {
    const int64_t day = 86400;
    const int64_t now = 1000 * day;
    check(sel_source_sync_due(0, 0, now, 30) == 1, "never synced is due");
    check(sel_source_sync_due(now - 29 * day, 0, now, 30) == 0, "synced 29 days ago is fresh");
    check(sel_source_sync_due(now - 30 * day, 0, now, 30) == 1, "synced 30 days ago is stale");
    check(sel_source_sync_due(now - 90 * day, now - 5 * day, now, 30) == 0, "dismissed 5 days ago hides a stale source");
    check(sel_source_sync_due(now - 90 * day, now - 31 * day, now, 30) == 1, "dismissal 31 days old offers again");
    check(sel_source_sync_due(0, now - 2 * day, now, 30) == 0, "never synced but dismissed 2 days ago is hidden");
    check(sel_source_sync_due(0, 0, now, 0) == 0, "days 0 turns the offer off for a never synced source");
    check(sel_source_sync_due(now - 900 * day, 0, now, 0) == 0, "days 0 turns the offer off for a stale source");
    check(sel_source_sync_due(now - 1 * day, 0, now, 1) == 1, "one day setting: exactly one day old is stale");
    if (fails) return 1;
    printf("source_sync_due_ok\n");
    return 0;
}
