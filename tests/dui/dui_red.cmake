# A red test of the desktop UI milestone (M3d): fails until the item it names lands. The item's real test replaces
# this script in tests/dui/CMakeLists.txt in the same commit that implements the item (and its id leaves
# tests/ratchet/unimplemented.txt).
# WHY (third field of tests/dui/items.txt) names the behaviour the plan's test asserts and what is missing in the code.
if(NOT WHY STREQUAL "")
  message(FATAL_ERROR "${NAME}: ${ID} is not implemented yet (missing feature: ${WHY}; see thoughts/shared/plans/2026-10-24-desktop-ui.md section 5)")
endif()
message(FATAL_ERROR "${NAME}: ${ID} is not implemented yet (missing feature: the desktop UI does not exist; see thoughts/shared/plans/2026-10-24-desktop-ui.md section 5)")
