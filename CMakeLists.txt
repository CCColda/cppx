cmake_minimum_required(VERSION 3.0.0)
project(buffer VERSION 1.0.0)

#---
option(BUFFER_BUILD_TEST "Build the tests for Buffer and Exception" OFF)
option(BUFFER_DEBUG "Build the debug features of the Buffer class" OFF)
#---

set(BUFFER_SRC_DIR src)
set(BUFFER_INC_DIR include)
set(BUFFER_TST_DIR test)

set(BUFFER_SRC_FILES
	${BUFFER_SRC_DIR}/Buffer.cpp
	${BUFFER_SRC_DIR}/Exception.cpp
)

set(BUFFER_INC_FILES
	${BUFFER_INC_DIR}/Buffer.hpp
	${BUFFER_INC_DIR}/Exception.hpp
)

set(BUFFER_TST_FILES
	${BUFFER_TST_DIR}/main.cpp
	${BUFFER_TST_DIR}/buffer.cpp
	${BUFFER_TST_DIR}/exception.cpp
)

#---
add_library(buffer STATIC)

add_library(cold::Buffer ALIAS buffer)
target_compile_features(buffer PRIVATE cxx_std_17)

target_include_directories(buffer PUBLIC ${BUFFER_INC_DIR})
target_sources(buffer PRIVATE ${BUFFER_SRC_FILES})

if (BUFFER_DEBUG)
	target_compile_definitions(buffer PUBLIC BUFFER_DEBUG)
endif()

#---

if (BUFFER_BUILD_TEST)
	target_compile_definitions(buffer PUBLIC BUFFER_DEBUG)

	Include(FetchContent)

	FetchContent_Declare(
		Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG        v3.0.1 # or a later release
	)

	FetchContent_MakeAvailable(Catch2)

	add_executable(buffer_test)
	target_sources(buffer_test PRIVATE ${BUFFER_TST_FILES})
	target_compile_features(buffer_test PRIVATE cxx_std_17)
	target_link_libraries(buffer_test PRIVATE cold::Buffer Catch2::Catch2WithMain)

	list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
	include(CTest)
	include(Catch)

	catch_discover_tests(buffer_test)
endif()
