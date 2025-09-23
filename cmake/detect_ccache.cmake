find_program(CCACHE_PATH ccache)

if (CCACHE_PATH AND DETECT_CCACHE)
	message(STATUS "Using ${CCACHE_PATH} to speed up builds because DETECT_CCACHE was ${DETECT_CCACHE} and we found ccache.")
	set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PATH}")
	set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PATH}")
endif()