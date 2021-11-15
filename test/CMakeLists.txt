cmake_minimum_required(VERSION 3.7)
project(test)

find_package(Boost 1.49 COMPONENTS
    unit_test_framework
)

if(NOT Boost_FOUND)
  message(WARNING "Boost not found, don't build tests")
  return()
endif(NOT Boost_FOUND)

add_executable(test_rvnbintrace_writer
  helpers.cpp
  test_trace_section_writers.cpp
  test_trace_writer.cpp
  test_cache_writer.cpp
)

target_link_libraries(test_rvnbintrace_writer
  PUBLIC
    Boost::boost

  PRIVATE
    rvnbintrace
    rvnmetadata::common
    rvnmetadata::bin
    Boost::unit_test_framework
)

target_compile_definitions(test_rvnbintrace_writer PRIVATE "BOOST_TEST_DYN_LINK")

add_test(rvnbintrace::writer test_rvnbintrace_writer)

add_executable(test_rvnbintrace_reader
  helpers.cpp
  test_trace_section_readers.cpp
  test_trace_reader.cpp
  test_cache_reader.cpp
)

target_link_libraries(test_rvnbintrace_reader
  PUBLIC
    Boost::boost

  PRIVATE
    rvnbintrace
    rvnmetadata::common
    rvnmetadata::bin
    Boost::unit_test_framework
)

target_compile_definitions(test_rvnbintrace_reader PRIVATE "BOOST_TEST_DYN_LINK")

add_test(rvnbintrace::reader test_rvnbintrace_reader)
