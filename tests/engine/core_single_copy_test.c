/* I1090: in MODULE mode (shared selecta_core), a consumer that names a folded static library before selecta_core must
 * still resolve every symbol of that library to the shared core. A private copy splits globals and registries: sources the
 * test registers never reach the engine. This test links selecta_library BEFORE selecta_core on purpose and checks, with
 * dladdr, that sel_source_register lives in the selecta_core image and not in the test executable. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "source.h"

int main(void) {
  Dl_info fn;
  Dl_info self;
  if (!dladdr((const void*)&sel_source_register, &fn) || !fn.dli_fname) {
    printf("dladdr failed for sel_source_register\n");
    return 1;
  }
  if (!dladdr((const void*)&main, &self) || !self.dli_fname) {
    printf("dladdr failed for main\n");
    return 1;
  }
  if (fn.dli_fbase == self.dli_fbase || !strstr(fn.dli_fname, "selecta_core")) {
    printf("sel_source_register resolved to %s, not the shared selecta_core: private registry copy\n", fn.dli_fname);
    return 1;
  }
  printf("core_single_copy_ok %s\n", fn.dli_fname);
  return 0;
}
