execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 30)
if(result STREQUAL "0" OR NOT diagnostic MATCHES "signed integer overflow")
    message(FATAL_ERROR "UBSan did not reject the control probe: ${result}\n${output}\n${diagnostic}")
endif()
