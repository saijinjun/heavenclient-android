//////////////////////////////////////////////////////////////////////////////////
//	OpenStory2-iOS — Platform Configuration
//	Central platform detection and feature flags.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

// Platform detection
// MS_PLATFORM_SDL is set by the build for the Android port (and its Linux
// desktop twin used for testing): SDL2 for window, input and audio, and
// OpenGL ES 3 for rendering.
#if defined(MS_PLATFORM_SDL)
	#define PLATFORM_SDL 1
	#if defined(__ANDROID__)
		#define PLATFORM_ANDROID 1
	#endif
	#ifndef USE_NX
		#define USE_NX
	#endif
	#ifndef USE_ASIO
		#define USE_ASIO
	#endif
	#ifndef USE_CRYPTO
		#define USE_CRYPTO
	#endif
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if TARGET_OS_IPHONE
		#define PLATFORM_IOS 1
	#else
		#define PLATFORM_MACOS 1
	#endif
#elif defined(_WIN32)
	#define PLATFORM_WINDOWS 1
#else
	#define PLATFORM_LINUX 1
#endif

// Feature flags for iOS
#ifdef PLATFORM_IOS
	#define USE_NX 1
	#define USE_ASIO 1
	#define USE_CRYPTO 1
#endif

// Feature flags for macOS. Winsock does not exist here, so networking always
// goes through Asio; NX and the packet crypto are the same as every other
// platform.
// Spelled with no replacement list on purpose: MapleStory.h defines USE_CRYPTO
// and USE_NX the same way, unguarded, and a redefinition only warns
// (-Wmacro-redefined) when the two token sequences differ. Every use is an
// #ifdef / #ifndef, so the absent value changes nothing.
#ifdef PLATFORM_MACOS
	#ifndef USE_NX
		#define USE_NX
	#endif
	#ifndef USE_ASIO
		#define USE_ASIO
	#endif
	#ifndef USE_CRYPTO
		#define USE_CRYPTO
	#endif
#endif

// Desktop GL target for the macOS port. Apple only exposes 4.1 Core (over
// Metal), so the legacy GLSL 120 shader path cannot be used there and the
// GLSL ES 3.00 shaders are promoted to 410 core instead.
#ifdef PLATFORM_MACOS
	#define GL_CONTEXT_VERSION_MAJOR 4
	#define GL_CONTEXT_VERSION_MINOR 1
	#define GL_CORE_PROFILE 1
#endif
