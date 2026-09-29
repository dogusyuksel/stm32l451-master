function(libcanard_add_to_target)
    set(options)
    set(one_value_args TARGET LIBCANARD_DIR DRIVER)
    cmake_parse_arguments(LIBCANARD
        "${options}"
        "${one_value_args}"
        ""
        ${ARGN}
    )

    if(NOT LIBCANARD_TARGET OR NOT LIBCANARD_LIBCANARD_DIR)
        message(FATAL_ERROR "libcanard_add_to_target requires TARGET and LIBCANARD_DIR")
    endif()

    target_sources(${LIBCANARD_TARGET} PRIVATE
        ${LIBCANARD_LIBCANARD_DIR}/canard.c
    )

    target_include_directories(${LIBCANARD_TARGET} PRIVATE
        ${LIBCANARD_LIBCANARD_DIR}
    )

    if(LIBCANARD_DRIVER)
        target_sources(${LIBCANARD_TARGET} PRIVATE
            ${LIBCANARD_LIBCANARD_DIR}/drivers/${LIBCANARD_DRIVER}/${LIBCANARD_DRIVER}.c
        )
        target_include_directories(${LIBCANARD_TARGET} PRIVATE
            ${LIBCANARD_LIBCANARD_DIR}/drivers/${LIBCANARD_DRIVER}
        )
    endif()
endfunction()
