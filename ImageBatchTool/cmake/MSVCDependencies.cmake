# Preserve the actual byte encoding of localized MSVC /showIncludes output.
# A mismatched prefix prevents Ninja from tracking header dependencies.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include-probe.h" "// Dependency prefix probe\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include-probe.cpp" "#include \"include-probe.h\"\n")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /utf-8 /showIncludes /EP /TP include-probe.cpp
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        OUTPUT_VARIABLE includes_output
        ERROR_VARIABLE includes_errors
        RESULT_VARIABLE includes_result
        ENCODING NONE
    )
    if(includes_result EQUAL 0 AND
       "${includes_output}${includes_errors}" MATCHES "(^|\n)([^\r\n]*: +)[A-Za-z]:[/\\\\]")
        set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
        set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
    else()
        message(FATAL_ERROR "Unable to detect MSVC header dependency prefix")
    endif()
endif()

