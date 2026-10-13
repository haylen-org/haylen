# The Windows project of the app. It takes the engine, the package, the native libraries and the values of `app.json` from `haylen/haylen.cmake`, which `haylen.py` writes every time it prepares the project, and everything else here belongs to the developer.
cmake_minimum_required(VERSION 3.28)
project(App LANGUAGES C CXX RC)

include(haylen/haylen.cmake)

# The executable of the app runs in the Windows subsystem, without a console, with the icon, the version and the manifest of `resources/app.rc`.
add_executable(app WIN32 resources/app.rc)
haylen_app(app)
set_target_properties(app PROPERTIES OUTPUT_NAME "${HAYLEN_APP_EXECUTABLE}")
# The manifest of `resources/app.rc` is complete, so the linkers of MSVC and clang-cl add none of their own.
if(MSVC)
  target_link_options(app PRIVATE /MANIFEST:NO)
endif()

# The installed app is the executable with its package in `app` and its native libraries next to it.
install(TARGETS app RUNTIME DESTINATION .)
install(DIRECTORY "${HAYLEN_PACKAGE_DIR}/" DESTINATION app)
install(DIRECTORY "${HAYLEN_NATIVE_DIR}/" DESTINATION .)

# Release builds sign the executable, the installer and its uninstaller when the environment names a certificate, as `signing.cmake` describes, and stay unsigned otherwise.
if(DEFINED ENV{HAYLEN_WINDOWS_CERTIFICATE} OR DEFINED ENV{HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT})
  install(CODE "
    if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Release\")
      execute_process(COMMAND \"${CMAKE_COMMAND}\" \"-DFILE=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${HAYLEN_APP_EXECUTABLE}.exe\" -P \"${CMAKE_CURRENT_SOURCE_DIR}/signing.cmake\" COMMAND_ERROR_IS_FATAL ANY)
    endif()
  ")
  set(CPACK_INNOSETUP_SETUP_SignTool "haylen")
  set(CPACK_INNOSETUP_EXECUTABLE_ARGUMENTS "\"/Shaylen=$q${CMAKE_COMMAND}$q -DFILE=$f -P $q${CMAKE_CURRENT_SOURCE_DIR}/signing.cmake$q\"")
else()
  install(CODE "
    if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Release\")
      message(STATUS \"The app stays unsigned, since the environment names no certificate. Name one with HAYLEN_WINDOWS_CERTIFICATE or HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT to sign it.\")
    endif()
  ")
endif()

# The installer, which Inno Setup makes through CPack, installs the app for the person or for every user, with a shortcut in the Start menu and an optional one on the desktop.
set(CPACK_GENERATOR INNOSETUP)
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_NAME "${HAYLEN_APP_NAME}")
set(CPACK_PACKAGE_VENDOR "${HAYLEN_APP_COMPANY}")
set(CPACK_PACKAGE_VERSION "${HAYLEN_APP_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${HAYLEN_APP_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${HAYLEN_APP_WEBSITE}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${HAYLEN_APP_NAME}")
set(CPACK_PACKAGE_FILE_NAME "${HAYLEN_APP_EXECUTABLE}-${HAYLEN_APP_VERSION}-setup")
set(CPACK_PACKAGE_EXECUTABLES "${HAYLEN_APP_EXECUTABLE}" "${HAYLEN_APP_NAME}")
set(CPACK_CREATE_DESKTOP_LINKS "${HAYLEN_APP_EXECUTABLE}")
set(CPACK_INNOSETUP_RUN_EXECUTABLES "${HAYLEN_APP_EXECUTABLE}")
set(CPACK_INNOSETUP_PROGRAM_MENU_FOLDER .)
set(CPACK_INNOSETUP_IGNORE_LICENSE_PAGE ON)
set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
set(CPACK_INNOSETUP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
set(CPACK_INNOSETUP_SETUP_AppId "${HAYLEN_APP_IDENTIFIER}")
set(CPACK_INNOSETUP_SETUP_AppCopyright "${HAYLEN_APP_COPYRIGHT}")
set(CPACK_INNOSETUP_SETUP_UninstallDisplayIcon "{app}\\${HAYLEN_APP_EXECUTABLE}.exe")
set(CPACK_INNOSETUP_SETUP_PrivilegesRequiredOverridesAllowed "dialog")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
  set(CPACK_INNOSETUP_ARCHITECTURE arm64)
  set(CPACK_INNOSETUP_SETUP_ArchitecturesAllowed arm64)
else()
  set(CPACK_INNOSETUP_ARCHITECTURE x64)
  set(CPACK_INNOSETUP_SETUP_ArchitecturesAllowed x64compatible)
endif()
set(CPACK_INNOSETUP_SETUP_ArchitecturesInstallIn64BitMode "${CPACK_INNOSETUP_SETUP_ArchitecturesAllowed}")
include(CPack)
