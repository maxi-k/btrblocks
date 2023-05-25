# -------------------------------------------------------------------------------
# Setup clang-tidy for specific targets
# -------------------------------------------------------------------------------
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-5.0 clang-tidy)

# Add target to lint the given source files
function(add_clang_tidy_target TARGET INPUT)
    if(NOT INPUT)
        add_custom_target(${TARGET})
        return()
    endif()

    # Remove duplicates & sort
    list(REMOVE_DUPLICATES INPUT)
    list(SORT INPUT)

    message(STATUS "LINT ${TARGET}: ${INPUT}")

    # Add target
    add_custom_target(${TARGET}
        COMMAND ${CMAKE_COMMAND} -E chdir
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CLANG_TIDY_EXECUTABLE}
                "-quiet"
                "-header-filter=${CMAKE_CURRENT_SOURCE_DIR}/include"
                "-p=${CMAKE_CURRENT_BINARY_DIR}"
                ${INPUT}
        COMMENT "Running ${TARGET}"
        VERBATIM)
endfunction()
