/* I1470: platform secret store for the saved DEViLBOX login (design doc 9.2, D-M3-12). Mac: one login Keychain item per
 * name behind a Touch ID (or Mac password) check; every other platform has none and keeps the 0600 token file.
 * Measured 2026-11-19 on the ad-hoc signed build: SecAccessControl items (biometry or user presence, data protection or
 * file keychain) fail with -34018 errSecMissingEntitlement, so the check is a LocalAuthentication gate in front of a
 * plain item (thoughts/spot/agent-reports/I1470-touchid.md). */
#ifndef SEL_KEYSTORE_H
#define SEL_KEYSTORE_H

#include <stddef.h>

/* 1 when this build has a secret store. */
int sel_keystore_available(void);
/* 1 when an item exists for name. Never asks the user. */
int sel_keystore_has(const char* name);
/* The secret into out (1). The first read in a run asks for Touch ID; later reads use the unlocked copy. A refused check
 * returns 0 and is not asked again this run. */
int sel_keystore_get(const char* name, char* out, size_t cap);
/* Stores the secret (replacing any) and keeps it unlocked for this run: the user just typed the password. */
int sel_keystore_put(const char* name, const char* secret);
/* Deletes the item and forgets the unlocked copy. */
int sel_keystore_del(const char* name);

#endif
