cmake_minimum_required(VERSION 3.22...4.3 FATAL_ERROR)

# Single source of truth for the version. The launchers and release
# packages read this file too.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HOTA_VERSION_STRING)
string(STRIP "${HOTA_VERSION_STRING}" HOTA_VERSION_STRING)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if(NOT HOTA_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?$")
	message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH[.TWEAK], got '${HOTA_VERSION_STRING}'")
endif()

project(hota-redux VERSION ${HOTA_VERSION_STRING} LANGUAGES C
	DESCRIPTION "Heart of The Alien Redux"
	HOMEPAGE_URL "")
set(PROJECT_BUGREPORT "https://github.com/carstene1ns/hota-redux/issues")

include(GNUInstallDirs)
include(CTest)

if(WIN32)
	enable_language(RC)
	set(HOTA_VERSION_RC
		"${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/src/alien.rc.in"
		"${CMAKE_CURRENT_BINARY_DIR}/alien.rc"
		@ONLY)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# set build type to debug, if not set
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
else()
	set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel.")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")

# external libs
find_package(SDL3 3.4 REQUIRED)
find_package(SDL3_mixer REQUIRED)

# OpenGL is only needed for the optional HD renderer; the game still
# builds and runs (SDL renderer) when it is missing.
set(HOTA_HAVE_OPENGL FALSE)
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
	set(HOTA_HAVE_OPENGL TRUE)
endif()

option(WANT_ASAN "build with address and undefined-behaviour sanitizers" OFF)
if(WANT_ASAN)
	message(STATUS "Building with address and undefined-behaviour sanitizers")
	add_compile_options(-fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all)
	add_link_options(-fno-omit-frame-pointer -fsanitize=address,undefined)
endif()

add_executable(alien
	src/main.c
	src/animation.c src/animation.h
	src/audio.c src/audio.h
	src/client.c src/client.h
	src/common.c src/common.h
	src/debug.c src/debug.h
	src/decode.c src/decode.h
	src/files.c src/files.h
	src/game2bin.c src/game2bin.h
	src/gl_render.c src/gl_render.h src/omniscale_shader.h
	src/keymap.c src/keymap.h
	src/lzss.c src/lzss.h
	src/parg.c src/parg.h
	src/platform.c src/platform.h
	src/render.c src/render.h
	src/rooms.c src/rooms.h
	src/screen.c src/screen.h
	src/sprites.c src/sprites.h
	src/upscale.c src/upscale.h
	src/vm.c src/vm.h)

if(WIN32)
	target_sources(alien PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/alien.rc")
endif()

target_compile_definitions(alien PRIVATE
	$<$<CONFIG:Debug>:ENABLE_DEBUG> HOTA_VERSION="${PROJECT_VERSION}")
if(HOTA_HAVE_OPENGL)
	target_compile_definitions(alien PRIVATE HAVE_OPENGL)
endif()
target_compile_options(alien PRIVATE -Wall -Wextra -Wstrict-prototypes -Wold-style-definition)

target_link_libraries(alien PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer)
if(HOTA_HAVE_OPENGL)
	target_link_libraries(alien PRIVATE OpenGL::GL)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(HOTA_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(HOTA_BUILD_FUZZERS)
	add_executable(fuzz_unlzss
		tests/fuzz/fuzz_unlzss.c src/lzss.c src/vm.c src/debug.c)
	target_include_directories(fuzz_unlzss PRIVATE src)
	target_compile_options(fuzz_unlzss PRIVATE
		-g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined)
	target_link_options(fuzz_unlzss PRIVATE
		-fsanitize=fuzzer,address,undefined)
endif()

if(BUILD_TESTING)
	add_executable(keymap_probe tests/keymap_probe.c src/keymap.c src/platform.c)
	target_include_directories(keymap_probe PRIVATE src)
	target_compile_options(keymap_probe PRIVATE -Wall -Wextra -Wstrict-prototypes -Wold-style-definition)
	target_link_libraries(keymap_probe PRIVATE SDL3::SDL3)

	set(HOTA_TEST_WORK "${CMAKE_CURRENT_BINARY_DIR}/test-work")
	file(MAKE_DIRECTORY "${HOTA_TEST_WORK}")

	add_test(NAME version COMMAND alien --version)
	add_test(NAME help COMMAND alien --help)
	add_test(NAME frames-invalid COMMAND alien --frames 0)
	add_test(NAME check-keys-default COMMAND alien --check-keys)
	add_test(NAME check-keys-sample COMMAND alien --check-keys --keys "${CMAKE_CURRENT_SOURCE_DIR}/tests/keys-sample.txt")
	add_test(NAME keymap-probe-default COMMAND keymap_probe)
	add_test(NAME keymap-probe-sample COMMAND keymap_probe "${CMAKE_CURRENT_SOURCE_DIR}/tests/keys-sample.txt")

	add_executable(unlzss_test
		tests/unlzss_test.c src/lzss.c src/vm.c src/debug.c)
	target_include_directories(unlzss_test PRIVATE src)
	target_compile_options(unlzss_test PRIVATE -Wall -Wextra -Wstrict-prototypes -Wold-style-definition)
	add_test(NAME unlzss-zero-stream COMMAND unlzss_test)
	set_tests_properties(unlzss-zero-stream PROPERTIES TIMEOUT 10)

	set_tests_properties(version help frames-invalid check-keys-default
		check-keys-sample keymap-probe-default keymap-probe-sample
		PROPERTIES WORKING_DIRECTORY "${HOTA_TEST_WORK}")
	set_tests_properties(version PROPERTIES
		PASS_REGULAR_EXPRESSION "^[0-9]+\\.[0-9]+\\.[0-9]+")
	set_tests_properties(frames-invalid PROPERTIES WILL_FAIL TRUE)

	# Game-data tests are opt-in because the ISO is copyrighted and is not
	# distributed with the source.
	set(HOTA_TEST_DATA_DIR "" CACHE PATH
		"Directory containing 'Heart Of The Alien (U).iso'")
	if(HOTA_TEST_DATA_DIR)
		set(HOTA_TEST_ISO "${HOTA_TEST_DATA_DIR}/Heart Of The Alien (U)")
		set(HOTA_TEST_ENV "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

		add_test(NAME room1-frames COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --frames 600
			--fastest --no-sound)
		set_tests_properties(room1-frames PROPERTIES
			WORKING_DIRECTORY "${HOTA_TEST_WORK}"
			ENVIRONMENT "${HOTA_TEST_ENV}")

		add_test(NAME quicksave-save COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --quicksave-test
			--fastest --no-sound)
		add_test(NAME quicksave-load COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --quickload-test
			--fastest --no-sound)
		set_tests_properties(quicksave-save quicksave-load PROPERTIES
			WORKING_DIRECTORY "${HOTA_TEST_WORK}"
			ENVIRONMENT "${HOTA_TEST_ENV}")
		set_tests_properties(quicksave-load PROPERTIES DEPENDS quicksave-save)

		add_test(NAME slot2-save COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --save-slot 2
			--fastest --no-sound)
		add_test(NAME slot2-load COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --load-slot 2
			--fastest --no-sound)
		set_tests_properties(slot2-save slot2-load PROPERTIES
			WORKING_DIRECTORY "${HOTA_TEST_WORK}"
			ENVIRONMENT "${HOTA_TEST_ENV}")
		set_tests_properties(slot2-load PROPERTIES DEPENDS slot2-save)

		add_test(NAME autosave-write COMMAND alien
			--iso "${HOTA_TEST_ISO}" --room 1 --frames 60 --no-sound)
		add_test(NAME autosave-resume COMMAND alien
			--iso "${HOTA_TEST_ISO}" --frames 30 --no-sound --debug)
		set_tests_properties(autosave-write autosave-resume PROPERTIES
			WORKING_DIRECTORY "${HOTA_TEST_WORK}"
			ENVIRONMENT "${HOTA_TEST_ENV}"
			TIMEOUT 60)
		set_tests_properties(autosave-resume PROPERTIES
			DEPENDS autosave-write
			PASS_REGULAR_EXPRESSION "resumed last session")

		if(WIN32)
			add_test(NAME suite-windows COMMAND powershell
				-ExecutionPolicy Bypass
				-File "${CMAKE_CURRENT_SOURCE_DIR}/tests/run-tests.ps1"
				-Exe $<TARGET_FILE:alien>
				-DataDir "${HOTA_TEST_DATA_DIR}")
		else()
			add_test(NAME suite-linux COMMAND bash
				"${CMAKE_CURRENT_SOURCE_DIR}/tests/run-tests-linux.sh")
			set_tests_properties(suite-linux PROPERTIES
				ENVIRONMENT "BIN=$<TARGET_FILE:alien>;DATA_DIR=${HOTA_TEST_DATA_DIR}")
		endif()
	endif()
endif()

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
set(HOTA_PACKAGE_BASENAME "hota-redux-${PROJECT_VERSION}")

if(WIN32)
	install(TARGETS alien RUNTIME DESTINATION .)
	install(FILES $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
		DESTINATION .)
	install(FILES
		README.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md VERSION
		play.bat play.ps1 play.py play.sh
		DESTINATION .)
	install(DIRECTORY LICENSES DESTINATION .)
elseif(APPLE)
	set_target_properties(alien PROPERTIES INSTALL_RPATH "@executable_path/lib")
	install(TARGETS alien RUNTIME DESTINATION .)
	install(FILES $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
		DESTINATION lib)
	install(FILES
		README.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md VERSION
		DESTINATION .)
	install(PROGRAMS play.sh play.py DESTINATION .)
	install(DIRECTORY LICENSES DESTINATION .)
else()
	# Portable layout: the executable and launchers live at the package
	# root and SDL is bundled in lib/ next to them.
	set_target_properties(alien PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
	install(TARGETS alien RUNTIME DESTINATION .)
	install(FILES $<TARGET_FILE:SDL3::SDL3>
		DESTINATION lib RENAME libSDL3.so.0)
	install(FILES $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
		DESTINATION lib RENAME libSDL3_mixer.so.0)
	install(FILES
		README.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md VERSION
		DESTINATION .)
	install(PROGRAMS play.sh play.py DESTINATION .)
	install(DIRECTORY LICENSES DESTINATION .)
	install(FILES res/alien.desktop DESTINATION share/applications)
	foreach(icon_size 16 24 32 48 64 96 128 256)
		install(FILES "res/icon${icon_size}.png"
			DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
			RENAME alien.png)
	endforeach()
endif()

set(CPACK_PACKAGE_NAME "hota-redux")
set(CPACK_PACKAGE_VENDOR "Jorge Salgueiro")
set(CPACK_PACKAGE_CONTACT "https://github.com/jorgesalgueiro/hota-redux")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Heart of The Alien Redux")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

if(WIN32)
	set(CPACK_GENERATOR "ZIP")
	set(CPACK_PACKAGE_FILE_NAME "${HOTA_PACKAGE_BASENAME}-win64")
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
elseif(APPLE)
	set(CPACK_GENERATOR "TGZ")
	set(CPACK_PACKAGE_FILE_NAME "${HOTA_PACKAGE_BASENAME}-macos")
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
else()
	set(CPACK_GENERATOR "TGZ")
	set(CPACK_PACKAGE_FILE_NAME "${HOTA_PACKAGE_BASENAME}-linux-x86_64")
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
endif()

include(CPack)
