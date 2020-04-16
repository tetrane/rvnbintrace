get_filename_component(RVNBINTRACE_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(rvnbinresource REQUIRED)
find_dependency(rvnmetadata REQUIRED)

if(NOT TARGET rvnbintrace)
	include("${RVNBINTRACE_CMAKE_DIR}/rvnbintrace-targets.cmake")
endif()
