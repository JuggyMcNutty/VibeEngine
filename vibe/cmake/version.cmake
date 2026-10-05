# The engine's version: the fork's commit and its date, stamped into every
# build, so a player can tell which engine they run (DeusExPlayer's
# GetDeusExVersion shows it at the main menu's bottom right).
#
# Included from CMakeLists.txt, it adds the target vibe_version, which runs on
# every build and rewrites <build>/vibe/vibe_version.h only when the text
# changes: a build after a new commit shows it, and recompiles only what reads
# it. Run as a script (cmake -P, with SOURCE_DIR and OUTPUT), it writes that
# header. "-dirty" marks a tree with uncommitted changes to tracked files; a
# tree without git reads "unknown".

if(CMAKE_SCRIPT_MODE_FILE)
	set(commit "")
	execute_process(COMMAND git -C "${SOURCE_DIR}" rev-parse --short=7 HEAD
		OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE failed ERROR_QUIET)
	if(failed OR commit STREQUAL "")
		set(version "unknown")
	else()
		execute_process(COMMAND git -C "${SOURCE_DIR}" log -1 --format=%cd --date=short
			OUTPUT_VARIABLE date OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
		execute_process(COMMAND git -C "${SOURCE_DIR}" status --porcelain --untracked-files=no
			OUTPUT_VARIABLE changes ERROR_QUIET)
		if(NOT changes STREQUAL "")
			string(APPEND commit "-dirty")
		endif()
		set(version "${commit} (${date})")
	endif()
	set(text "// Written by vibe/cmake/version.cmake at build time.\n#define VIBE_VERSION \"${version}\"\n")
	set(old "")
	if(EXISTS "${OUTPUT}")
		file(READ "${OUTPUT}" old)
	endif()
	if(NOT old STREQUAL text)
		file(WRITE "${OUTPUT}" "${text}")
	endif()
	return()
endif()

set(VIBE_VERSION_DIR "${CMAKE_BINARY_DIR}/vibe")
add_custom_target(vibe_version
	COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
		"-DOUTPUT=${VIBE_VERSION_DIR}/vibe_version.h" -P "${CMAKE_CURRENT_LIST_FILE}"
	BYPRODUCTS "${VIBE_VERSION_DIR}/vibe_version.h"
	COMMENT "VibeEngine's version"
	VERBATIM)
add_dependencies(SurrealCommon vibe_version)
target_include_directories(SurrealCommon PRIVATE "${VIBE_VERSION_DIR}")
