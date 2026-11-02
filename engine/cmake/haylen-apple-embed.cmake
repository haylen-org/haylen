# Copies frameworks, app extensions or resource bundles into an Apple app bundle after its link, keeping the links inside them, and signs the code among them like the app.
# Run with `-DITEMS=<paths separated by |> -DDESTINATION=<folder> -DSIGN=<ON|OFF>`.
# Xcode gives the identity it signs the app with and whether it signs at all, while other generators sign ad hoc.

string(REPLACE "|" ";" items "${ITEMS}")
set(identity "$ENV{EXPANDED_CODE_SIGN_IDENTITY}")
if(identity STREQUAL "")
  set(identity "-")
endif()

file(MAKE_DIRECTORY "${DESTINATION}")
foreach(item IN LISTS items)
  get_filename_component(name "${item}" NAME)
  file(REMOVE_RECURSE "${DESTINATION}/${name}")
  execute_process(COMMAND ditto "${item}" "${DESTINATION}/${name}" COMMAND_ERROR_IS_FATAL ANY)
  if(SIGN AND NOT "$ENV{CODE_SIGNING_ALLOWED}" STREQUAL "NO")
    execute_process(COMMAND codesign --force --sign "${identity}" --timestamp=none --preserve-metadata=identifier,entitlements,flags "${DESTINATION}/${name}" COMMAND_ERROR_IS_FATAL ANY)
  endif()
endforeach()
