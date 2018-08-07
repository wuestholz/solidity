if (BOOGIE_BIN)
  find_file(BOOGIE_EXE Boogie.exe "${BOOGIE_BIN}")
else() 
  find_file(BOOGIE_EXE Boogie.exe)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BOOGIE DEFAULT_MSG BOOGIE_EXE)
