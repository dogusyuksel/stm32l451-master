function(libcsp2_build)
    set(options)
    set(one_value_args TARGET LIBCSP_DIR)
    set(multi_value_args INCLUDES)
    cmake_parse_arguments(LIBCSP
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT LIBCSP_TARGET OR NOT LIBCSP_LIBCSP_DIR)
        message(FATAL_ERROR "libcsp2_build requires TARGET and LIBCSP_DIR")
    endif()

    list(JOIN LIBCSP_INCLUDES "," libcsp_includes)

    add_custom_target(libcsp_build ALL
        COMMAND ./waf configure
            --with-os=freertos
            --toolchain=arm-none-eabi-
            --enable-custom-changes
            --with-max-bind-port 61
            --enable-promisc
            --with-rtable-size 32
            --enable-rtable
            --includes
                ${libcsp_includes}
        COMMAND ./waf build
        WORKING_DIRECTORY ${LIBCSP_LIBCSP_DIR}
        COMMENT "Building libcsp2 with waf..."
    )

    add_dependencies(${LIBCSP_TARGET} libcsp_build)
endfunction()
