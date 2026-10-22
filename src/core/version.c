#include <selecta/api.h>

#define SEL_STR2(x) #x
#define SEL_STR(x) SEL_STR2(x)

const char* sel_version_string(void) {
    return SEL_VERSION_STRING " api " SEL_STR(1);
}
