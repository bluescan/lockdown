cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW)

# Set default build-type (AKA the configuration in other IDEs).
set(CMAKE_BUILD_TYPE_INIT Release)

# Setup Release and Debug build-types (only).
# No reason to set CMAKE_CONFIGURATION_TYPES if it's not a multiconfig generator
# Also no reason mess with CMAKE_BUILD_TYPE if it's a multiconfig generator.
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (isMultiConfig)
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
else()
	if (NOT DEFINED CMAKE_BUILD_TYPE)
		message(STATUS "Lockdown -- Default to Release build.")
		set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Build Type" FORCE)
	endif()
	message(STATUS "Lockdown -- Build type set to: ${CMAKE_BUILD_TYPE}")
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose Build Type")

	# Set the valid options for cmake-gui drop-down list. CMake tools for vscode does not (but should) respect this.
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

# This include specifies the project and version.
include("Src/Version.cmake.h")
project(lockdown VERSION "${LOCKDOWN_VERSION}" LANGUAGES C CXX)
message(STATUS "Lockdown -- Name ${PROJECT_NAME} Version ${PROJECT_VERSION}")
message(STATUS "Lockdown -- Major:${PROJECT_VERSION_MAJOR} Minor:${PROJECT_VERSION_MINOR} Revision:${PROJECT_VERSION_PATCH}")
message(STATUS "Lockdown -- CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

# Find Git module.
find_package(Git)
if (Git_FOUND)
	message(STATUS "Lockdown -- Git found: ${GIT_EXECUTABLE}")
endif()

# We want a better default for install prefix. It is bad form to be modifying
# system files from a cmake build of anything. Really quite surprised someone
# thinks the cmake defaults are good.
# message(STATUS "init=${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT}")
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "..." FORCE)
endif()
message(STATUS "Lockdown -- InstallPrefix ${CMAKE_INSTALL_PREFIX}")

include(FetchContent)

# Grab the Tacent library from github at configure time. Set desired options here first.
if (CMAKE_SYSTEM_NAME MATCHES Windows)
	option(TACENT_UTF16_API_CALLS "Build Tacent With UTF16 API Calls" Off)
endif()

# See https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html#fetchcontent-and-find-package-integration
# for how FIND_PACKAGE_ARGS allows FetchContent_MakeAvailable to try find_package() first.
FetchContent_Declare(
	tacent
	GIT_REPOSITORY https://github.com/bluescan/tacent.git
	# GIT_TAG 4941d5455d16768652aea0cd67381b71cb23ea64
	# GIT_TAG v0.8.18
	FIND_PACKAGE_ARGS NAMES tacent
)
FetchContent_MakeAvailable(tacent)

# After this call you can use variables tacent_POPULATED (a bool), tacent_BINARY_DIR, and tacent_SOURCE_DIR
FetchContent_GetProperties(tacent)
message(STATUS "Lockdown -- tacent_POPULATED: ${tacent_POPULATED}")
message(STATUS "Lockdown -- tacent_BINARY_DIR: ${tacent_BINARY_DIR}")
message(STATUS "Lockdown -- tacent_SOURCE_DIR: ${tacent_SOURCE_DIR}")

# Files needed to create executable.
add_executable(
	${PROJECT_NAME}
	Src/Lockdown.cpp
	Src/Version.cmake.h
	Src/Version.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Res/Lockdown.rc
)

# Include directories needed to build.
target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Src
		${CMAKE_CURRENT_SOURCE_DIR}/Res
		${CMAKE_CURRENT_SOURCE_DIR}/Lib/libgamepad/include
)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		ARCHITECTURE_X64
		$<$<CONFIG:Debug>:CONFIG_DEBUG>
		$<$<CONFIG:Release>:CONFIG_RELEASE>
		$<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_DEPRECATE>
		PLATFORM_WINDOWS

		# These shouldn't actually be necessary as there are no direct Windows API calls
		# in TacentView (they are abstracted away by the Tacent libraries). But just in case
		# anything in the viewer were to call an OS-level function, these enable the UTF-16
		# versions if the TACENT_UTF16_API_CALLS option is set.
		# $<$<BOOL:${PACKAGE_PORTABLE}>:PACKAGE_PORTABLE>
		$<$<BOOL:${TACENT_UTF16_API_CALLS}>:UNICODE>					# C++	UFF-16
		$<$<BOOL:${TACENT_UTF16_API_CALLS}>:_UNICODE>					# C		UTF-16
		$<$<BOOL:${TACENT_UTF16_API_CALLS}>:TACENT_UTF16_API_CALLS>
)

# Set compiler option flags based on specific compiler and configuration.
target_compile_options(
	${PROJECT_NAME}
	PRIVATE
		# MSVC compiler. Warning /utf-8 is needed for MSVC to support UTF-8 source files. Basically u8 string literals won't work without it.
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W2 /GS /Gy /Zc:wchar_t /Gm- /Zc:inline /fp:precise /WX- /Zc:forScope /Gd /FC>

		# Clang compiler.
		$<$<CXX_COMPILER_ID:Clang>:-Wno-switch>

		# GNU compiler.
		$<$<CXX_COMPILER_ID:GNU>:-Wno-unused-result>
		$<$<CXX_COMPILER_ID:GNU>:-Wno-multichar>

		# Clang and GNU.
		$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-format-security>

		$<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-O0>
		$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/Od>
		$<$<AND:$<CONFIG:Release>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-O2>
		$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# This is how you set things like CMAKE_DEBUG_POSTFIX for a target.
if (MSVC)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		# DEBUG_POSTFIX "d"												# Add a 'd' before the extension for debug builds.
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"	# Use multithreaded or multithreaded-debug runtime on windows.
		WIN32_EXECUTABLE TRUE
		# More prop-value pairs here.
	)
endif()

# Dependencies.
target_link_libraries(${PROJECT_NAME} PRIVATE
	Foundation Math System Comctl32.lib
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/Lib/libgamepad/debug/gamepad.lib>
	$<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/Lib/libgamepad/release/gamepad.lib>
)

if (MSVC)
	target_link_options(
		${PROJECT_NAME}
		PRIVATE
		# $<IF:$<BOOL:${TACENT_UTF16_API_CALLS}>,/ENTRY:wmainCRTStartup,/ENTRY:mainCRTStartup>
		# "/SUBSYSTEM:CONSOLE"
		"/SUBSYSTEM:WINDOWS"
	)
	if (CMAKE_BUILD_TYPE MATCHES Debug)
		target_link_options(
			${PROJECT_NAME}
			PRIVATE
				"/NODEFAULTLIB:LIBCMT.lib"
	)
	endif()
endif()

# Install
set(LOCKDOWN_INSTALL_DIR "${CMAKE_BINARY_DIR}/LockdownInstall")
message(STATUS "Lockdown -- ${PROJECT_NAME} will be installed to ${LOCKDOWN_INSTALL_DIR}")

# Installation.

install(
	TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION "${LOCKDOWN_INSTALL_DIR}"
)
