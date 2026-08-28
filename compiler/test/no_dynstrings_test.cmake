foreach(DQ_DYNSTRING_CASE TEST_STR TEST_CONCAT TEST_ANYVALUE)
  execute_process(
    COMMAND "${DQ_COMPILER}" --no-dynstrings "-D${DQ_DYNSTRING_CASE}" -c
            -o "${DQ_OUTPUT}" "${DQ_SOURCE}"
    RESULT_VARIABLE DQ_RESULT
    OUTPUT_VARIABLE DQ_OUTPUT_TEXT
    ERROR_VARIABLE DQ_ERROR_TEXT)
  if(DQ_RESULT EQUAL 0 OR NOT "${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}" MATCHES "ERROR\\(DynStringsDisabled\\)")
    message(FATAL_ERROR
      "${DQ_DYNSTRING_CASE} did not produce DynStringsDisabled:\n${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}")
  endif()
endforeach()
