# FindGSL.cmake
# Provides:
#	- Targets: GSL::gsl, GSL::gslcblas
#	- Vars:    GSL_FOUND, GSL_INCLUDE_DIRS, GSL_LIBRARIES, GSL_CBLAS_LIBRARIES, GSL_VERSION (from pkg-config if available)

cmake_minimum_required(VERSION 3.16)

set(GSL_ROOT "" CACHE PATH "Root prefix where GSL is installed (e.g., C:/gsl or /opt/homebrew/opt/gsl)")

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_GSL QUIET gsl)
endif()

set(_HINT_INC)
set(_HINT_LIB)

# pkg-config hints (Linux/macOS/MSYS2)
if(PC_GSL_FOUND)
	list(APPEND _HINT_INC ${PC_GSL_INCLUDEDIR} ${PC_GSL_INCLUDE_DIRS})
	list(APPEND _HINT_LIB ${PC_GSL_LIBDIR} ${PC_GSL_LIBRARY_DIRS})
endif()

# User override
if(GSL_ROOT)
	list(APPEND _HINT_INC "${GSL_ROOT}/include")
	list(APPEND _HINT_LIB "${GSL_ROOT}/lib" "${GSL_ROOT}/lib64")
endif()

# Cross-compile sysroot
if(CMAKE_SYSROOT)
	list(APPEND _HINT_INC
		"${CMAKE_SYSROOT}/usr/include"
		"${CMAKE_SYSROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}"
	)
	list(APPEND _HINT_LIB
		"${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
		"${CMAKE_SYSROOT}/usr/lib"
		"${CMAKE_SYSROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
		"${CMAKE_SYSROOT}/lib"
	)
endif()

# macOS: Homebrew + /usr/local + stable "opt" symlink
if(APPLE)
	list(APPEND _HINT_INC "/opt/homebrew/opt/gsl/include" "/opt/homebrew/include" "/usr/local/include")
	list(APPEND _HINT_LIB "/opt/homebrew/opt/gsl/lib" "/opt/homebrew/lib" "/usr/local/lib")
endif()

# Windows: common locations (use 8.3 short paths to avoid parentheses in env vars)
if(WIN32)
	list(APPEND _HINT_INC
		"C:/gsl/include"
		"C:/PROGRA~1/gsl/include"
		"C:/PROGRA~2/gsl/include"
		"$ENV{ProgramFiles}/gsl/include"
	)
	list(APPEND _HINT_LIB
		"C:/gsl/lib"
		"C:/PROGRA~1/gsl/lib"
		"C:/PROGRA~2/gsl/lib"
		"$ENV{ProgramFiles}/gsl/lib"
	)
endif()

# Locate headers
find_path(GSL_INCLUDE_DIR
	NAMES gsl/gsl_math.h
	HINTS ${_HINT_INC}
)

# Locate libs (GSL + CBLAS; allow both names)
find_library(GSL_LIBRARY
	NAMES gsl libgsl
	HINTS ${_HINT_LIB}
)
find_library(GSL_CBLAS_LIBRARY
	NAMES gslcblas cblas libgslcblas libcblas
	HINTS ${_HINT_LIB}
)

# Version (optional) via pkg-config
if(PC_GSL_VERSION)
	set(GSL_VERSION "${PC_GSL_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSL
	REQUIRED_VARS GSL_INCLUDE_DIR GSL_LIBRARY GSL_CBLAS_LIBRARY
	VERSION_VAR GSL_VERSION
)

if(GSL_FOUND)
	set(GSL_INCLUDE_DIRS "${GSL_INCLUDE_DIR}")
	set(GSL_LIBRARIES "${GSL_LIBRARY}" "${GSL_CBLAS_LIBRARY}")
	set(GSL_CBLAS_LIBRARIES "${GSL_CBLAS_LIBRARY}")

	# Primary target: GSL::gsl (links both gsl + cblas, adds -lm on non-Windows)
	if(NOT TARGET GSL::gsl)
		add_library(GSL::gsl INTERFACE IMPORTED)
		set_target_properties(GSL::gsl PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${GSL_INCLUDE_DIRS}"
			INTERFACE_LINK_LIBRARIES "${GSL_LIBRARY};${GSL_CBLAS_LIBRARY};$<$<NOT:$<PLATFORM_ID:Windows>>:m>"
		)
	endif()

	# Optional companion alias if you want to refer to CBLAS alone
	if(NOT TARGET GSL::gslcblas)
		add_library(GSL::gslcblas INTERFACE IMPORTED)
		set_target_properties(GSL::gslcblas PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${GSL_INCLUDE_DIRS}"
			INTERFACE_LINK_LIBRARIES "${GSL_CBLAS_LIBRARY};$<$<NOT:$<PLATFORM_ID:Windows>>:m>"
		)
	endif()

	message(STATUS "Resolved GSL include dir: ${GSL_INCLUDE_DIRS}")
	message(STATUS "Resolved GSL libraries: ${GSL_LIBRARIES}")
else()
	message(WARNING "GSL not found. Set GSL_ROOT or CMAKE_PREFIX_PATH if installed in a nonstandard prefix.")
endif()