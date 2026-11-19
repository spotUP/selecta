/* I1470: login Keychain behind a LocalAuthentication check (keystore.h). The ad-hoc signed app cannot create
 * SecAccessControl items (-34018), so Touch ID is asked here before the plain item's secret is handed out.
 * Upgrade once the app has a Developer ID and a keychain-access-groups entitlement: add kSecAttrAccessControl with
 * kSecAccessControlBiometryCurrentSet and kSecUseDataProtectionKeychain, and drop the gate. */
#include "keystore.h"

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define SERVICE @"Selecta DEViLBOX login"

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static char g_name[512];
static char g_secret[1024]; /* the unlocked copy for g_name */
static int g_state;         /* 0 locked, 1 unlocked, 2 refused this run */

static NSMutableDictionary* query(const char* name) {
    return [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : SERVICE,
        (id)kSecAttrAccount : [NSString stringWithUTF8String:name]
    } mutableCopy];
}

/* Caller holds g_mu. */
static void remember(const char* name, const char* secret, int state) {
    memset(g_secret, 0, sizeof g_secret);
    snprintf(g_name, sizeof g_name, "%s", name);
    snprintf(g_secret, sizeof g_secret, "%s", secret);
    g_state = state;
}

int sel_keystore_available(void) {
    return 1;
}

int sel_keystore_has(const char* name) {
    if (name == NULL) return 0;
    @autoreleasepool {
        return SecItemCopyMatching((__bridge CFDictionaryRef)query(name), NULL) == errSecSuccess;
    }
}

/* Touch ID, with the Mac password as the fallback (no sensor, lid closed). Blocks the calling thread only: the reply
 * comes on a LocalAuthentication queue. */
static int unlock(void) {
    LAContext* ctx = [LAContext new];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block BOOL ok = NO;

    [ctx evaluatePolicy:LAPolicyDeviceOwnerAuthentication
        localizedReason:@"unlock your saved DEViLBOX login"
                  reply:^(BOOL success, NSError* error) {
                      (void)error;
                      ok = success;
                      dispatch_semaphore_signal(done);
                  }];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    return ok == YES;
}

int sel_keystore_get(const char* name, char* out, size_t cap) {
    int ok = 0;

    if (name == NULL || out == NULL || cap == 0u) return 0;
    out[0] = '\0';
    pthread_mutex_lock(&g_mu);
    if (strcmp(g_name, name) == 0 && g_state == 1) {
        ok = strlen(g_secret) < cap;
        if (ok) memcpy(out, g_secret, strlen(g_secret) + 1u);
    } else if (strcmp(g_name, name) != 0 || g_state != 2) {
        @autoreleasepool {
            NSMutableDictionary* q = query(name);
            LAContext* quiet = [LAContext new];
            CFTypeRef data = NULL;

            quiet.interactionNotAllowed = YES; /* no system Keychain dialog: the gate below is the only prompt */
            q[(id)kSecReturnData] = @YES;
            q[(id)kSecUseAuthenticationContext] = quiet;
            if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &data) == errSecSuccess && data != NULL) {
                NSData* d = (__bridge_transfer NSData*)data;
                char secret[sizeof g_secret];

                if (d.length > 0u && d.length < sizeof secret) {
                    memcpy(secret, d.bytes, d.length);
                    secret[d.length] = '\0';
                    if (unlock()) {
                        remember(name, secret, 1);
                        ok = d.length < cap;
                        if (ok) memcpy(out, secret, d.length + 1u);
                    } else {
                        remember(name, "", 2);
                    }
                    memset(secret, 0, sizeof secret);
                }
            }
        }
    }
    pthread_mutex_unlock(&g_mu);
    return ok;
}

int sel_keystore_put(const char* name, const char* secret) {
    OSStatus st;

    if (name == NULL || secret == NULL || strlen(secret) >= sizeof g_secret) return 0;
    pthread_mutex_lock(&g_mu);
    @autoreleasepool {
        NSMutableDictionary* q = query(name);

        (void)SecItemDelete((__bridge CFDictionaryRef)q);
        q[(id)kSecValueData] = [NSData dataWithBytes:secret length:strlen(secret)];
        q[(id)kSecAttrLabel] = SERVICE;
        st = SecItemAdd((__bridge CFDictionaryRef)q, NULL);
    }
    if (st == errSecSuccess) remember(name, secret, 1);
    pthread_mutex_unlock(&g_mu);
    return st == errSecSuccess;
}

int sel_keystore_del(const char* name) {
    OSStatus st;

    if (name == NULL) return 0;
    pthread_mutex_lock(&g_mu);
    @autoreleasepool {
        st = SecItemDelete((__bridge CFDictionaryRef)query(name));
    }
    if (strcmp(g_name, name) == 0) remember("", "", 0);
    pthread_mutex_unlock(&g_mu);
    return st == errSecSuccess || st == errSecItemNotFound;
}
