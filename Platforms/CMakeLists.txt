cmake_minimum_required (VERSION 3.10)

add_library(Diligent-PlatformInterface INTERFACE)

target_include_directories(Diligent-PlatformInterface INTERFACE interface)

set(PLATFORM_INTERFACE_HEADERS
    ../interface/FileSystem.hpp
    ../interface/Intrinsics.hpp
    ../interface/PlatformDebug.hpp
    ../interface/PlatformDefinitions.h
    ../interface/PlatformMisc.hpp
    ../interface/NativeWindow.h
)

add_subdirectory(Basic)

if(PLATFORM_WIN32)
	add_subdirectory(Win32)
	add_library(Diligent-TargetPlatform ALIAS Diligent-Win32Platform)
elseif(PLATFORM_UNIVERSAL_WINDOWS)
	add_subdirectory(UWP)
	add_library(Diligent-TargetPlatform ALIAS Diligent-UniversalWindowsPlatform)
elseif(PLATFORM_ANDROID)
	add_subdirectory(Android)
	add_library(Diligent-TargetPlatform ALIAS Diligent-AndroidPlatform)
elseif(PLATFORM_LINUX)
	add_subdirectory(Linux)
	add_library(Diligent-TargetPlatform ALIAS Diligent-LinuxPlatform)
elseif(PLATFORM_APPLE)
	add_subdirectory(Apple)
	add_library(Diligent-TargetPlatform ALIAS Diligent-ApplePlatform)
elseif(PLATFORM_WEB)
	add_subdirectory(Emscripten)
	add_library(Diligent-TargetPlatform ALIAS Diligent-EmscriptenPlatform)
    target_link_options(Diligent-EmscriptenPlatform PUBLIC "SHELL: -s DISABLE_EXCEPTION_CATCHING=0'")
else()
	message(FATAL_ERROR "No PLATFORM_XXX variable defined. Make sure that 'DiligentCore' folder is processed first")
endif()
