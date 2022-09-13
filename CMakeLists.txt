cmake_minimum_required(VERSION 3.0.0)
project(cppx
	VERSION      1.0.1
	DESCRIPTION  "C++ Extended"
	HOMEPAGE_URL "https://github.com/CCColda/cppx#readme"
	LANGUAGES    "CXX"
)

#---
option(CPPX_BUILD_TEST "Build the tests for cppx" OFF)
option(CPPX_BUFFER_DEBUG "Build the debug features of the Buffer class" OFF)
option(CPPX_BUFFER_BUILTINS "Use __builtin functions" OFF)
#---

set(CPPX_SRC_DIR src)
set(CPPX_INC_DIR include)
set(CPPX_TST_DIR test)

set(CPPX_SRC_FILES
	${CPPX_SRC_DIR}/cppxBuffer.cpp
	${CPPX_SRC_DIR}/cppxException.cpp
)

set(CPPX_INC_FILES
	${CPPX_INC_DIR}/cppxBuffer.hpp
	${CPPX_INC_DIR}/cppxException.hpp
)

set(CPPX_TST_FILES
	${CPPX_TST_DIR}/buffer.test.cpp
	${CPPX_TST_DIR}/exception.test.cpp
)

#---
add_library(cppx STATIC)

add_library(colda::cppx ALIAS cppx)
target_compile_features(cppx PRIVATE cxx_std_17)

target_include_directories(cppx PUBLIC ${CPPX_INC_DIR})
target_sources(cppx PRIVATE ${CPPX_SRC_FILES})

if (CPPX_BUFFER_DEBUG)
	target_compile_definitions(cppx PUBLIC CPPX_BUFFER_DEBUG)
endif()

if (CPPX_BUFFER_BUILTINS)
	target_compile_definitions(cppx PUBLIC CPPX_BUFFER_BUILTINS)
endif()

#---

if (CPPX_BUILD_TEST)
	target_compile_definitions(cppx PUBLIC CPPX_BUFFER_DEBUG)

	Include(FetchContent)

	FetchContent_Declare(
		Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG        v3.0.1
	)

	FetchContent_MakeAvailable(Catch2)

	add_executable(cppx_test)
	target_sources(cppx_test PRIVATE ${CPPX_TST_FILES})
	target_compile_features(cppx_test PRIVATE cxx_std_17)
	target_link_libraries(cppx_test PRIVATE colda::cppx Catch2::Catch2WithMain)

	list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
	include(CTest)
	include(Catch)

	catch_discover_tests(cppx_test)
endif()
