foreach(DQ_OPT IN ITEMS s z)
  set(DQ_OBJECT "${DQ_OUTPUT_DIR}/size_O${DQ_OPT}.o")
  execute_process(
    COMMAND "${DQ_COMPILER}" --target=arm_m7f-bare --no-use-sys "-O${DQ_OPT}" -ir -c
            -o "${DQ_OBJECT}" "${DQ_SOURCE}"
    RESULT_VARIABLE DQ_RESULT
    OUTPUT_VARIABLE DQ_STDOUT
    ERROR_VARIABLE DQ_STDERR
  )
  if(NOT DQ_RESULT EQUAL 0)
    message(FATAL_ERROR "dq-comp -O${DQ_OPT} failed:\n${DQ_STDOUT}${DQ_STDERR}")
  endif()

  if(NOT DQ_STDOUT MATCHES "optsize")
    message(FATAL_ERROR "dq-comp -O${DQ_OPT} did not add the LLVM optsize attribute")
  endif()
  if(DQ_OPT STREQUAL "z" AND NOT DQ_STDOUT MATCHES "minsize")
    message(FATAL_ERROR "dq-comp -Oz did not add the LLVM minsize attribute")
  endif()
endforeach()
