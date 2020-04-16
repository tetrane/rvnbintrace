add_executable(rvn_file_trace_reader
  file_trace_reader.cpp
)

target_link_libraries(rvn_file_trace_reader
  PUBLIC
    rvnbintrace
)

include(GNUInstallDirs)
install(TARGETS rvn_file_trace_reader
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
