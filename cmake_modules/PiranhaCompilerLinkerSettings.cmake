INCLUDE(CheckCXXCompilerFlag)
INCLUDE(CheckTypeSize)

message(STATUS "The C++ compiler ID is: ${CMAKE_CXX_COMPILER_ID}")

# Clang detection:
# http://stackoverflow.com/questions/10046114/in-cmake-how-can-i-test-if-the-compiler-is-clang
# http://www.cmake.org/cmake/help/v2.8.10/cmake.html#variable:CMAKE_LANG_COMPILER_ID
IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(CMAKE_COMPILER_IS_CLANGXX 1)
ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	set(CMAKE_COMPILER_IS_INTELXX 1)
endif()

macro(PIRANHA_CHECK_ENABLE_CXX_FLAG flag)
	set(PIRANHA_CHECK_CXX_FLAG)
	check_cxx_compiler_flag("${flag}" PIRANHA_CHECK_CXX_FLAG)
	if(PIRANHA_CHECK_CXX_FLAG)
		message(STATUS "Enabling the '${flag}' compiler flag.")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
	else()
		message(STATUS "Disabling the '${flag}' compiler flag.")
	endif()
	unset(PIRANHA_CHECK_CXX_FLAG CACHE)
endmacro()

macro(PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG flag)
	set(PIRANHA_CHECK_DEBUG_CXX_FLAG)
	check_cxx_compiler_flag("${flag}" PIRANHA_CHECK_DEBUG_CXX_FLAG)
	if(PIRANHA_CHECK_DEBUG_CXX_FLAG)
		message(STATUS "Enabling the '${flag}' debug compiler flag.")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${flag}")
	else()
		message(STATUS "Disabling the '${flag}' debug compiler flag.")
	endif()
	unset(PIRANHA_CHECK_DEBUG_CXX_FLAG CACHE)
endmacro()

# Macro to detect the 128-bit unsigned integer type available on some compilers.
macro(PIRANHA_CHECK_UINT128_T)
	MESSAGE(STATUS "Looking for a 128-bit unsigned integer type.")
	# NOTE: for now we support only the GCC integer.
	# NOTE: use this instead of the unsigned __int128. See:
	# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=50454
	CHECK_TYPE_SIZE("__uint128_t" PIRANHA_UINT128_T)
	IF(PIRANHA_UINT128_T)
		MESSAGE(STATUS "128-bit unsigned integer type detected.")
		SET(PIRANHA_HAVE_UINT128_T "#define PIRANHA_UINT128_T __uint128_t")
	ELSE()
		MESSAGE(STATUS "No 128-bit unsigned integer type detected.")
	ENDIF()
endmacro(PIRANHA_CHECK_UINT128_T)

# Setup the C++ standard flag. We try C++14 first, if not available we go with C++11.
macro(PIRANHA_SET_CPP_STD_FLAG)
	# This is valid for GCC, clang and Intel. I think that MSVC has the std version hardcoded.
	if(CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_INTELXX)
		set(PIRANHA_CHECK_CXX_FLAG)
		check_cxx_compiler_flag("-std=c++14" PIRANHA_CHECK_CXX_FLAG)
		if(PIRANHA_CHECK_CXX_FLAG)
			message(STATUS "C++14 supported by the compiler, enabling.")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
		else()
			message(STATUS "C++14 is not supported by the compiler, C++11 will be enabled.")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
		endif()
	endif()
endmacro()

# Configuration for GCC.
IF(CMAKE_COMPILER_IS_GNUCXX)
	MESSAGE(STATUS "GNU compiler detected, checking version.")
	TRY_COMPILE(GCC_VERSION_CHECK ${CMAKE_BINARY_DIR} "${CMAKE_SOURCE_DIR}/cmake_modules/gcc_check_version.cpp")
	IF(NOT GCC_VERSION_CHECK)
		MESSAGE(FATAL_ERROR "Unsupported GCC version, please upgrade your compiler.")
	ENDIF(NOT GCC_VERSION_CHECK)
	MESSAGE(STATUS "GCC version is ok.")
	# The trouble here is that -g (which implies -g2) results in ICE in some tests and in
	# some pyranha exposition cases. We just append -g1 here, which overrides the default -g.
	# For MinGW, we disable debug info altogether.
	if (MINGW)
		message(STATUS "Forcing the debug flag to -g0 for GCC.")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g0")
	else()
		message(STATUS "Forcing the debug flag to -g1 for GCC.")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g1")
	endif()
	# Set the standard flag.
	PIRANHA_SET_CPP_STD_FLAG()
	# Enable libstdc++ pedantic debug mode in debug builds.
	# NOTE: this is disabled by default, as it requires the c++ library to be compiled with this
	# flag enabled in order to be reliable (and this is not the case usually):
	# http://gcc.gnu.org/onlinedocs/libstdc++/manual/debug_mode.html
# 	IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
# 		ADD_DEFINITIONS(-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC)
# 	ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
	# A problem with MinGW is that we run into a "too many sections" quite often. Recently, MinGW
	# added a -mbig-obj for the assembler that solves this, but this is available only in recent versions.
	# Other tentative workarounds include disabling debug information, enabling global inlining, use -Os and others.
	# The -mthreads flag is apparently needed for thread-safe exception handling.
	if(MINGW)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads -Wa,-mbig-obj")
		# The debug build can run into a "file is too large" error. Work around by
		# compiling for small size.
		set(CMAKE_CXX_FLAGS_DEBUG "-Os")
		#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finline-functions")
	endif()
	PIRANHA_CHECK_UINT128_T()
	# Color diagnostic available since GCC 4.9.
	PIRANHA_CHECK_ENABLE_CXX_FLAG(-fdiagnostics-color=auto)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

IF(CMAKE_COMPILER_IS_CLANGXX)
	MESSAGE(STATUS "Clang compiler detected, checking version.")
	TRY_COMPILE(CLANG_VERSION_CHECK ${CMAKE_BINARY_DIR} "${CMAKE_SOURCE_DIR}/cmake_modules/clang_check_version.cpp")
	IF(NOT CLANG_VERSION_CHECK)
		MESSAGE(FATAL_ERROR "Unsupported Clang version, please upgrade your compiler.")
	ENDIF(NOT CLANG_VERSION_CHECK)
	MESSAGE(STATUS "Clang version is ok.")
	# Set the standard flag.
	PIRANHA_SET_CPP_STD_FLAG()
	# This used to be necessary with earlier versions of Clang which
	# were not completely compatible with GCC's stdlib. Nowadays it seems
	# unnecessary on most platforms.
	# PIRANHA_CHECK_ENABLE_CXX_FLAG(-stdlib=libc++)
	PIRANHA_CHECK_UINT128_T()
	# For now it seems like -Wshadow from clang behaves better than GCC's, just enable it here
	# for the time being.
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wshadow)
	# Clang is better at this flag than GCC, which emits a questionable warning when compiling
	# the Python bindings.
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Werror)
ENDIF(CMAKE_COMPILER_IS_CLANGXX)

if(CMAKE_COMPILER_IS_INTELXX)
	message(STATUS "Intel compiler detected, checking version.")
	try_compile(INTEL_VERSION_CHECK ${CMAKE_BINARY_DIR} "${CMAKE_SOURCE_DIR}/cmake_modules/intel_check_version.cpp")
	if(NOT INTEL_VERSION_CHECK)
		message(FATAL_ERROR "Unsupported Intel compiler version, please upgrade your compiler.")
	endif()
	message(STATUS "Intel compiler version is ok.")
	# Set the standard flag.
	PIRANHA_SET_CPP_STD_FLAG()
	# The diagnostic from the Intel compiler can be wrong and a pain in the ass.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -diag-disable 2304,2305,1682,2259,3373")
	PIRANHA_CHECK_UINT128_T()
endif()

# Common configuration for GCC, Clang and Intel.
if(CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_INTELXX)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wall)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wextra)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wnon-virtual-dtor)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wnoexcept)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wlogical-op)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wconversion)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wdeprecated)
	# In the serialization work, we started hitting the template recursive instantiation
	# limit on clang. This limit is supposed to be at least 1024 in C++11, but for some reason
	# clang sets this to 256, and gcc to 900.
	PIRANHA_CHECK_ENABLE_CXX_FLAG(-ftemplate-depth=1024)
	# NOTE: this can be useful, but at the moment it triggers lots of warnings in type traits.
	# Keep it in mind for the next time we touch type traits.
	# PIRANHA_CHECK_ENABLE_CXX_FLAG(-Wold-style-cast)
	# NOTE: disable this for now, as it results in a lot of clutter from Boost.
	# PIRANHA_CHECK_ENABLE_CXX_FLAG(-Wzero-as-null-pointer-constant)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-pedantic-errors)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wdisabled-optimization)
	PIRANHA_CHECK_ENABLE_CXX_FLAG(-fvisibility-inlines-hidden)
	PIRANHA_CHECK_ENABLE_CXX_FLAG(-fvisibility=hidden)
	# This is useful when the compiler decides the template backtrace is too verbose.
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-ftemplate-backtrace-limit=0)
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-fstack-protector-all)
	# This became available in GCC at one point.
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wodr)
	# This is available only in clang at the moment apparently.
	PIRANHA_CHECK_ENABLE_DEBUG_CXX_FLAG(-Wunsequenced)
endif()
