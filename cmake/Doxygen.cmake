# Doxygen.cmake
# Defines sjhopengl_setup_doxygen() to create a 'doxygen' build target.

function(sjhopengl_setup_doxygen)
    find_package(Doxygen QUIET)

    if(NOT DOXYGEN_FOUND)
        message(STATUS "Doxygen not found — 'doxygen' target will not be available")
        return()
    endif()

    set(DOXYGEN_INPUT_DIR  "${PROJECT_SOURCE_DIR}/src")
    set(DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/doc")
    set(DOXYGEN_PAGES_DIR  "${PROJECT_SOURCE_DIR}/doc/pages")

    configure_file(
        "${PROJECT_SOURCE_DIR}/doc/Doxyfile.in"
        "${PROJECT_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} "${PROJECT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endfunction()
