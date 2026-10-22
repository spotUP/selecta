# The generated test module must match its pinned SHA-256 byte for byte.
# Variables: MOD PINNED
if(NOT EXISTS "${MOD}")
  message(FATAL_ERROR "test module not generated: ${MOD}")
endif()
file(SHA256 "${MOD}" hash)
file(READ "${PINNED}" pinned)
string(SUBSTRING "${pinned}" 0 64 pinned)
if(NOT hash STREQUAL pinned)
  message(FATAL_ERROR "selecta-test.mod hash ${hash} differs from ${PINNED} (${pinned})")
endif()
