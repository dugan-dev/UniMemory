if(NOT DEFINED PROGRAM OR NOT DEFINED MODE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "PROGRAM, MODE and EXPECTED are required")
endif()
execute_process(COMMAND "${PROGRAM}" "${MODE}" RESULT_VARIABLE actual
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT "${actual}" STREQUAL "${EXPECTED}")
    message(FATAL_ERROR "Expected exit ${EXPECTED}, got ${actual}: ${output}${error}")
endif()
