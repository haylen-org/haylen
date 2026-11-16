# Signs the file that `FILE` names with `signtool` of the Windows SDK and an RFC 3161 time stamp, so the signature outlives the certificate:
#   cmake -DFILE=<file> -P signing.cmake
# The environment gives the certificate, which never belongs in the project:
#   HAYLEN_WINDOWS_CERTIFICATE             A PFX file with the certificate and its private key.
#   HAYLEN_WINDOWS_CERTIFICATE_PASSWORD    The password of the PFX file.
#   HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT  Instead of a file, the SHA-1 thumbprint of a certificate in the store of the user, such as one on a hardware token.
#   HAYLEN_WINDOWS_TIMESTAMP_URL           The time stamp server of the authority that issued the certificate.
#   HAYLEN_WINDOWS_SIGNTOOL                The signtool to use, when the one on PATH or in the Windows SDK is not the right one.

if(NOT FILE)
  message(FATAL_ERROR "Name the file to sign with -DFILE=<file>.")
endif()
if(NOT DEFINED ENV{HAYLEN_WINDOWS_TIMESTAMP_URL})
  message(FATAL_ERROR "Name the time stamp server of the authority that issued the certificate in HAYLEN_WINDOWS_TIMESTAMP_URL, so the signature stays valid after the certificate expires.")
endif()

if(DEFINED ENV{HAYLEN_WINDOWS_SIGNTOOL})
  set(signtool "$ENV{HAYLEN_WINDOWS_SIGNTOOL}")
else()
  set(program_files "ProgramFiles(x86)")
  file(GLOB sdk_signtools "$ENV{${program_files}}/Windows Kits/10/bin/*/x64/signtool.exe")
  list(SORT sdk_signtools COMPARE NATURAL ORDER DESCENDING)
  find_program(signtool signtool HINTS ${sdk_signtools})
  if(NOT signtool AND sdk_signtools)
    list(GET sdk_signtools 0 signtool)
  endif()
  if(NOT signtool)
    message(FATAL_ERROR "The command \"signtool\" was not found. Install the Windows SDK, or name the command in HAYLEN_WINDOWS_SIGNTOOL.")
  endif()
endif()

if(DEFINED ENV{HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT})
  set(certificate /sha1 "$ENV{HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT}")
else()
  set(certificate /f "$ENV{HAYLEN_WINDOWS_CERTIFICATE}" /p "$ENV{HAYLEN_WINDOWS_CERTIFICATE_PASSWORD}")
endif()

execute_process(COMMAND "${signtool}" sign /fd sha256 /td sha256 /tr "$ENV{HAYLEN_WINDOWS_TIMESTAMP_URL}" ${certificate} "${FILE}" RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "The command \"signtool\" could not sign \"${FILE}\".")
endif()
message(STATUS "Signed \"${FILE}\".")
