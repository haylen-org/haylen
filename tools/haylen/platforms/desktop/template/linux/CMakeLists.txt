# The Linux project of the app, which the tooling generates from `haylen.json` and the installed desktop SDK. The file `app.cmake` holds the values of the app and builds its executable with `haylen_add_app`.
cmake_minimum_required(VERSION 3.28...4.4)
project(App LANGUAGES C CXX)

include(app.cmake)

# The executable finds its native libraries next to it and in `lib`, and the C++ library of the compiler goes into it, so the app runs on distributions older than the one that built it.
set_target_properties(main PROPERTIES OUTPUT_NAME "${HAYLEN_APP_EXECUTABLE}" BUILD_RPATH "$ORIGIN;$ORIGIN/lib" INSTALL_RPATH "$ORIGIN;$ORIGIN/lib")
target_link_options(main PRIVATE "LINKER:--enable-new-dtags" -static-libstdc++ -static-libgcc)

# The installed app lives in `lib/<executable>` with its package in `app` and its native libraries in `lib`, which its `RUNPATH` names, and `bin/<executable>` links to it, so desktops start it by name.
set(app_folder "lib/${HAYLEN_APP_EXECUTABLE}")
install(TARGETS main RUNTIME DESTINATION "${app_folder}")
install(DIRECTORY "${HAYLEN_APP_CONTENT}/" DESTINATION "${app_folder}/app")
install(DIRECTORY "${HAYLEN_APP_NATIVE}/" DESTINATION "${app_folder}/lib")
file(CREATE_LINK "../${app_folder}/${HAYLEN_APP_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/${HAYLEN_APP_EXECUTABLE}" SYMBOLIC)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${HAYLEN_APP_EXECUTABLE}" DESTINATION bin)

# The desktop entry, the AppStream metainfo and the icons take the identifier of the app, which is also the class of its window.
install(FILES resources/app.desktop DESTINATION share/applications RENAME "${HAYLEN_APP_IDENTIFIER}.desktop")
install(FILES resources/app.metainfo.xml DESTINATION share/metainfo RENAME "${HAYLEN_APP_IDENTIFIER}.metainfo.xml")
foreach(size 16 24 32 48 64 128 256 512)
  install(FILES "resources/icons/${size}.png" DESTINATION "share/icons/hicolor/${size}x${size}/apps" RENAME "${HAYLEN_APP_IDENTIFIER}.png")
endforeach()
install(FILES resources/icons/app.svg DESTINATION share/icons/hicolor/scalable/apps RENAME "${HAYLEN_APP_IDENTIFIER}.svg")

# The packages, which CPack makes: a `.deb` for Debian, Ubuntu and the distributions based on them, and an AppImage that runs on any distribution without installing.
set(CPACK_GENERATOR DEB AppImage)
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PACKAGE_NAME "${HAYLEN_APP_EXECUTABLE}")
set(CPACK_PACKAGE_VENDOR "${HAYLEN_APP_COMPANY}")
set(CPACK_PACKAGE_VERSION "${HAYLEN_APP_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${HAYLEN_APP_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "${HAYLEN_APP_COMPANY}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${HAYLEN_APP_WEBSITE}")
set(CPACK_PACKAGE_FILE_NAME "${HAYLEN_APP_EXECUTABLE}-${HAYLEN_APP_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_ICON "${HAYLEN_APP_IDENTIFIER}.svg")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION games)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# The engine loads GIO for the theme and the monitors of the system, and the sound servers, when it runs, so the package recommends them.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libglib2.0-0t64 | libglib2.0-0, libpulse0 | libasound2t64 | libasound2")
# The tool appimagetool looks for AppStream metadata only under its legacy name, while the metainfo keeps the current one.
set(CPACK_APPIMAGE_NO_APPSTREAM ON)
include(CPack)
