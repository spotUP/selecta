# Source trees of the pinned submodules (third_party/nuade, third_party/libopenmpt).
#
# A linked git worktree (agent worktrees under .claude/worktrees) checks out no
# submodule contents: the directories exist but are empty. Checking the
# submodules out inside the worktree is no answer, because `git worktree
# remove` refuses a worktree that contains submodules, and a symlink in place
# of the gitlink makes `git status` fail ("expected submodule path ... not to
# be a symbolic link"). So the build reads the trees from a path instead:
# SELECTA_NUADE_DIR / SELECTA_OPENMPT_DIR default to the worktree's own copy,
# and when that copy is empty in a linked worktree, to the main checkout's copy
# of the same submodule. The main checkout's copy is the pinned commit unless
# someone moved it there; a worktree that changes a pin must check out that
# submodule itself and pass the path.

function(_sel_submodule_dir VAR REL MARKER)
  set(_own "${PROJECT_SOURCE_DIR}/${REL}")
  set(_dir "${_own}")
  if(NOT EXISTS "${_own}/${MARKER}")
    find_package(Git QUIET)
    if(GIT_FOUND)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --path-format=absolute --git-common-dir
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE _common OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc ERROR_QUIET)
      if(_rc EQUAL 0 AND _common)
        get_filename_component(_main "${_common}" DIRECTORY)
        if(NOT _main STREQUAL PROJECT_SOURCE_DIR AND EXISTS "${_main}/${REL}/${MARKER}")
          set(_dir "${_main}/${REL}")
          message(STATUS "${REL} is empty in this worktree; using ${_dir}")
        endif()
      endif()
    endif()
  endif()
  set(${VAR} "${_dir}" CACHE PATH "Source tree of the ${REL} submodule")
endfunction()

# Called where a submodule is used (not every build uses both).
function(sel_require_submodule VAR REL MARKER)
  if(NOT EXISTS "${${VAR}}/${MARKER}")
    message(FATAL_ERROR "${REL} has no contents at ${${VAR}}: run "
      "`git submodule update --init ${REL}` in the main checkout, or pass -D${VAR}=<path>")
  endif()
endfunction()

_sel_submodule_dir(SELECTA_NUADE_DIR third_party/nuade cmake/nuade_players.cmake)
_sel_submodule_dir(SELECTA_OPENMPT_DIR third_party/libopenmpt Makefile)
