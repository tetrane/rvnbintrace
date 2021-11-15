cmake_minimum_required(VERSION 3.7)
project(rvnbintrace)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

option(BUILD_SHARED_LIBS "Set to ON to build shared libraries; OFF for static libraries." OFF)
option(WARNING_AS_ERROR "Set to ON to build with -Werror" ON)

option(BUILD_TEST_COVERAGE "Set to ON to build while generating coverage information. Will put source on the build directory." OFF)

find_package(rvnbinresource REQUIRED)
find_package(rvnmetadata REQUIRED)

add_library(rvnbintrace
  src/section_reader.cpp
  src/section_writer.cpp

  src/trace_section_readers.cpp
  src/trace_section_writers.cpp
  src/trace_reader.cpp
  src/trace_writer.cpp

  src/cache_section_readers.cpp
  src/cache_section_writers.cpp
  src/cache_reader.cpp
  src/cache_writer.cpp
)

target_compile_options(rvnbintrace PRIVATE -W -Wall -Wextra -Wmissing-include-dirs -Wunknown-pragmas -Wpointer-arith -Wmissing-field-initializers -Wno-multichar -Wreturn-type)

if(WARNING_AS_ERROR)
  target_compile_options(rvnbintrace PRIVATE -Werror)
endif()

if(BUILD_TEST_COVERAGE)
  target_compile_options(rvnbintrace PRIVATE -g -O0 --coverage -fprofile-arcs -ftest-coverage)
  target_link_libraries(rvnbintrace PRIVATE gcov)
endif()

target_include_directories(rvnbintrace
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rvnbintrace
  PUBLIC
    rvnbinresource
    rvnmetadata::common
    rvnmetadata::bin
)

set(PUBLIC_HEADERS
  include/cache_reader.h
  include/cache_writer.h
  include/cache_section_readers.h
  include/cache_section_writers.h
  include/cache_sections.h

  include/reader_errors.h
  include/writer_errors.h

  include/section_reader.h
  include/section_writer.h

  include/trace_reader.h
  include/trace_writer.h
  include/trace_section_readers.h
  include/trace_section_writers.h
  include/trace_sections.h
)

set_target_properties(rvnbintrace PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}"
  POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS rvnbintrace
  EXPORT rvnbintrace-export
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

install(EXPORT rvnbintrace-export
  FILE
    rvnbintrace-targets.cmake
  DESTINATION
    ${CMAKE_INSTALL_DATADIR}/cmake/rvnbintrace
)

install(FILES cmake/rvnbintrace-config.cmake
  DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/rvnbintrace
)

add_subdirectory(bin)

enable_testing()
add_subdirectory(test)
