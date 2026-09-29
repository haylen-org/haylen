# Merges static libraries and object files into one static library with the archiver of the platform.
# Run with -DKIND=libtool|lib|mri -DTOOL=<archiver> -DOUTPUT=<library> -DINPUTS=<file with one input per line>.

file(STRINGS "${INPUTS}" inputs)
file(REMOVE "${OUTPUT}")

if(KIND STREQUAL "libtool")
  execute_process(COMMAND "${TOOL}" -static -no_warning_for_no_symbols -o "${OUTPUT}" ${inputs} RESULT_VARIABLE result ERROR_VARIABLE messages)
  # Libraries from different projects share object names, which libtool reports although both objects are kept.
  string(REGEX REPLACE "[^\n]*same member name[^\n]*\n?" "" messages "${messages}")
elseif(KIND STREQUAL "lib")
  execute_process(COMMAND "${TOOL}" /NOLOGO "/OUT:${OUTPUT}" ${inputs} RESULT_VARIABLE result ERROR_VARIABLE messages)
else()
  set(script "CREATE ${OUTPUT}\n")
  foreach(input IN LISTS inputs)
    if(input MATCHES "\\.(a|lib)$")
      string(APPEND script "ADDLIB ${input}\n")
    else()
      string(APPEND script "ADDMOD ${input}\n")
    endif()
  endforeach()
  string(APPEND script "SAVE\nEND\n")
  file(WRITE "${OUTPUT}.mri" "${script}")
  execute_process(COMMAND "${TOOL}" -M INPUT_FILE "${OUTPUT}.mri" RESULT_VARIABLE result ERROR_VARIABLE messages)
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Merging ${OUTPUT} failed: ${messages}")
endif()
if(messages)
  message(WARNING "${messages}")
endif()
