foreach(DQ_EXCEPTION_CASE TEST_TRY TEST_FINALLY TEST_VALUE_RAISE TEST_RERAISE)
  execute_process(
    COMMAND "${DQ_COMPILER}" --no-exceptions "-D${DQ_EXCEPTION_CASE}" -c
            -o "${DQ_OUTPUT}" "${DQ_SOURCE}"
    RESULT_VARIABLE DQ_RESULT
    OUTPUT_VARIABLE DQ_OUTPUT_TEXT
    ERROR_VARIABLE DQ_ERROR_TEXT
  )
  if(DQ_RESULT EQUAL 0 OR NOT "${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}" MATCHES "ERROR\\(ExceptionsDisabled\\)")
    message(FATAL_ERROR
      "${DQ_EXCEPTION_CASE} did not produce ExceptionsDisabled:\n${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}")
  endif()
endforeach()

execute_process(
  COMMAND "${DQ_COMPILER}" --target=arm_m7f-bare --no-use-sys -c
          -o "${DQ_OUTPUT}" "${DQ_SYS_SOURCE}"
  RESULT_VARIABLE DQ_RESULT
  OUTPUT_VARIABLE DQ_OUTPUT_TEXT
  ERROR_VARIABLE DQ_ERROR_TEXT
)
if(DQ_RESULT EQUAL 0 OR NOT "${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}" MATCHES "ERROR\\(VsUnknown\\)")
  message(FATAL_ERROR
    "--no-use-sys did not remove the implicit prelude:\n${DQ_OUTPUT_TEXT}${DQ_ERROR_TEXT}")
endif()
