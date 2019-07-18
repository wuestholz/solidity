set(
	sources
	CommandLineInterface.cpp CommandLineInterface.h
	main.cpp
)

add_executable(solc ${sources})
target_link_libraries(solc PRIVATE solidity Boost::boost Boost::program_options)

include(GNUInstallDirs)
install(TARGETS solc DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(SOLC_LINK_STATIC AND UNIX AND NOT APPLE)
	# Produce solc as statically linked binary (includes C/C++ standard libraries)
	# This is not supported on macOS, see
	# https://developer.apple.com/library/content/qa/qa1118/_index.html.
	set_target_properties(
		solc PROPERTIES
		LINK_FLAGS -static
		LINK_SEARCH_START_STATIC ON
		LINK_SEARCH_END_STATIC ON
	)
endif()

# Find Boogie
include(FindBoogie)
find_package(BOOGIE QUIET)
if (BOOGIE_EXE)
  if ((NOT BOOGIE_DEFAULT_SOLVER) AND USE_Z3)
    find_program(Z3_BINARY z3)
    if (Z3_BINARY)
      set(BOOGIE_DEFAULT_SOLVER "'z3'")
    endif()
  endif()
  if ((NOT BOOGIE_DEFAULT_SOLVER) AND USE_CVC4)
    find_program(CVC4_BINARY cvc4)
    if (CVC4_BINARY)
      set(BOOGIE_DEFAULT_SOLVER "'cvc4'")
    endif()
  endif()
  if (NOT BOOGIE_DEFAULT_SOLVER)
    set(BOOGIE_DEFAULT_SOLVER "None")
  endif()
  message(STATUS "Boogie default solver: ${BOOGIE_DEFAULT_SOLVER}")
  configure_file("solc-verify.py.in" "solc-verify.py")
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/solc-verify.py" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
