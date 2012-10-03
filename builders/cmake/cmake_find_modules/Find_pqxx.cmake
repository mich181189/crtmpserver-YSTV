FIND_PATH(PQXX_INCLUDE_PATH
	NAMES
		pqxx/pqxx
	PATHS
		${TOOLCHAIN_HEADER_PATH}
		/opt/local/include
		/sw/include
		/usr/include
		/usr/local/include
		/usr/local/ssl/include
		NO_DEFAULT_PATH)

IF($ENV{COMPILE_STATIC} MATCHES "1")
	SET(CMAKE_FIND_LIBRARY_SUFFIXES_OLD ${CMAKE_FIND_LIBRARY_SUFFIXES})
	SET(CMAKE_FIND_LIBRARY_SUFFIXES .a .so .dylib ${CMAKE_FIND_LIBRARY_SUFFIXES})
ENDIF($ENV{COMPILE_STATIC} MATCHES "1")

FIND_LIBRARY(PQXX_LIBRARY_PATH
	NAMES
		pqxx
	PATHS
		${TOOLCHAIN_LIBRARY_PATH}
		/opt/local/lib64
		/opt/local/lib
		/sw/lib64
		/sw/lib
		/lib64
		/usr/lib64
		/usr/local/lib64
		/lib/x86_64-linux-gnu
		/usr/lib/x86_64-linux-gnu
		/opt/local/lib64
		/lib
		/usr/lib
		/usr/local/lib
		/lib/i386-linux-gnu
		/usr/lib/i386-linux-gnu
		/usr/local/ssl/lib
		NO_DEFAULT_PATH)



MESSAGE(STATUS "PQXX_INCLUDE_PATH: ${PQXX_INCLUDE_PATH}")
MESSAGE(STATUS "PQXX_LIBRARY_PATH: ${PQXX_LIBRARY_PATH}")

IF(PQXX_INCLUDE_PATH)
	SET(PQXX_FOUND 1 CACHE STRING "Set to 1 if pqxx is found, 0 otherwise")
	MESSAGE(STATUS "Looking for pqxx headers - found")
ELSE(PQXX_INCLUDE_PATH)
	SET(PQXX_FOUND 0 CACHE STRING "Set to 1 if pqxx is found, 0 otherwise")
	MESSAGE(FATAL_ERROR "Looking for pqxx headers - not found")
ENDIF(PQXX_INCLUDE_PATH)

IF(PQXX_LIBRARY_PATH)
	SET(PQXX_FOUND 1 CACHE STRING "Set to 1 if pqxx is found, 0 otherwise")
	MESSAGE(STATUS "Looking for pqxx library - found")
ELSE(PQXX_LIBRARY_PATH)
	SET(PQXX_FOUND 0 CACHE STRING "Set to 1 if pqxx is found, 0 otherwise")
	MESSAGE(FATAL_ERROR "Looking for pqxx library - not found")
ENDIF(PQXX_LIBRARY_PATH)

MARK_AS_ADVANCED(PQXX_FOUND)

