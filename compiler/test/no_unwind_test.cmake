execute_process(
  COMMAND "${DQ_NM}" --undefined-only "${DQ_BINARY}"
  RESULT_VARIABLE DQ_RESULT
  OUTPUT_VARIABLE DQ_NM_OUTPUT
  ERROR_VARIABLE DQ_NM_ERROR
)
if(NOT DQ_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Could not inspect exception-free ARM output:\n${DQ_NM_OUTPUT}${DQ_NM_ERROR}")
endif()

if("${DQ_NM_OUTPUT}" MATCHES "(__aeabi_unwind|_Unwind_|__gnu_Unwind)")
  message(FATAL_ERROR
    "Exception-free ARM output references the unwinder:\n${DQ_NM_OUTPUT}")
endif()
