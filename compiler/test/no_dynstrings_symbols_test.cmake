execute_process(
  COMMAND "${DQ_NM}" "${DQ_OBJECT}"
  RESULT_VARIABLE DQ_RESULT
  OUTPUT_VARIABLE DQ_NM_OUTPUT
  ERROR_VARIABLE DQ_NM_ERROR)

if(NOT DQ_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Could not inspect dynamic-string-free ARM output:\n${DQ_NM_OUTPUT}${DQ_NM_ERROR}")
endif()

if("${DQ_NM_OUTPUT}" MATCHES "DynStr|ODynStr")
  message(FATAL_ERROR
    "Dynamic-string-free ARM output references the dynamic string runtime:\n${DQ_NM_OUTPUT}")
endif()
