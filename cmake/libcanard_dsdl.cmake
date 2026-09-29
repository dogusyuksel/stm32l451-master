function(libcanard_generate_dsdl)
    set(options)
    set(one_value_args TARGET DSDL_ROOT GENERATED_DIR COMPILER)
    set(multi_value_args INCLUDE_DIRS)
    cmake_parse_arguments(DSDL
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT DSDL_TARGET OR NOT DSDL_DSDL_ROOT OR NOT DSDL_GENERATED_DIR OR NOT DSDL_COMPILER)
        message(FATAL_ERROR "libcanard_generate_dsdl requires TARGET, DSDL_ROOT, GENERATED_DIR and COMPILER")
    endif()

    set(dsdl_incdir_args)
    foreach(include_dir IN LISTS DSDL_INCLUDE_DIRS)
        list(APPEND dsdl_incdir_args --incdir "${include_dir}")
    endforeach()

    file(GLOB_RECURSE dsdl_files CONFIGURE_DEPENDS
        "${DSDL_DSDL_ROOT}/*.uavcan"
    )

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DSDL_GENERATED_DIR}"
        COMMAND "${DSDL_COMPILER}"
                "${DSDL_DSDL_ROOT}"
                ${dsdl_incdir_args}
                --outdir "${DSDL_GENERATED_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE dsdl_generate_result
    )

    if(NOT dsdl_generate_result EQUAL 0)
        message(FATAL_ERROR "DSDL generation failed")
    endif()

    file(GLOB_RECURSE generated_sources CONFIGURE_DEPENDS
        "${DSDL_GENERATED_DIR}/*.c"
    )

    set_source_files_properties(${generated_sources} PROPERTIES
        COMPILE_OPTIONS "-Wno-error=sign-conversion"
    )

    add_custom_target(generate_${DSDL_TARGET}_dsdl
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DSDL_GENERATED_DIR}"
        COMMAND "${DSDL_COMPILER}"
                "${DSDL_DSDL_ROOT}"
                ${dsdl_incdir_args}
                --outdir "${DSDL_GENERATED_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS ${dsdl_files}
        COMMENT "Generating DSDL sources"
    )

    add_dependencies(${DSDL_TARGET} generate_${DSDL_TARGET}_dsdl)
    target_sources(${DSDL_TARGET} PRIVATE ${generated_sources})
    target_include_directories(${DSDL_TARGET} PRIVATE ${DSDL_GENERATED_DIR})
endfunction()
