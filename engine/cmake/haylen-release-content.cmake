# Builds the protected release of an app package with the content tool and lays out what ships with the app, for haylen_add_app with HAYLEN_PROTECTED_RELEASE.
# Reads TOOL, PACKAGE, IDENTIFIER, KEYS, PROFILE, BUILD, RELEASE, PREVIOUS, CACHE, SHIPPED, PACKS, SERVER, BOOT and STAMP. PREVIOUS names the folder of the last release that shipped, or is empty. BOOT is empty except for web pages, which download the manifests and the app domain it holds before the app starts and stream the rest of RELEASE.

# The keys of an app are created once on the machine of its developer, while continuous integration receives them and never creates them, since every release of an app needs the same keys.
if(NOT EXISTS "${KEYS}/keys.json")
  if(DEFINED ENV{HAYLEN_KEYS_DIR} AND NOT "$ENV{HAYLEN_KEYS_DIR}" STREQUAL "")
    message(FATAL_ERROR "The key folder \"${KEYS}\" holds no keys of \"${IDENTIFIER}\". Give \"HAYLEN_KEYS_DIR\" a copy of the key folders made where the releases of the app were built first.")
  endif()
  execute_process(COMMAND "${TOOL}" keys create "${KEYS}" --identifier "${IDENTIFIER}" COMMAND_ERROR_IS_FATAL ANY)
  message(WARNING "Created the keys of the protected releases of \"${IDENTIFIER}\" in \"${KEYS}\". Back up this folder, since every release of the app needs its keys, and give continuous integration a copy through \"HAYLEN_KEYS_DIR\".")
endif()

# A new release reuses the shards of the one before it, the one that shipped when PREVIOUS names it, so an unchanged asset keeps its shard byte for byte.
set(staging "${RELEASE}.new")
file(REMOVE_RECURSE "${staging}")
set(previous)
if(NOT PREVIOUS STREQUAL "")
  set(previous --previous "${PREVIOUS}")
elseif(EXISTS "${RELEASE}/app.hmanifest")
  set(previous --previous "${RELEASE}")
endif()
execute_process(COMMAND "${TOOL}" build "${PACKAGE}" --keys "${KEYS}" --profile "${PROFILE}" --build "${BUILD}" --output "${staging}" --cache "${CACHE}" ${previous} COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE_RECURSE "${RELEASE}")
file(RENAME "${staging}" "${RELEASE}")

# The shards of named packs go to the stores that deliver them, and with a server the shards that only later deliveries use stay for the server.
file(REMOVE_RECURSE "${SHIPPED}" "${PACKS}")
set(deferred --ship-deferred)
if(SERVER)
  set(deferred)
endif()
execute_process(COMMAND "${TOOL}" stage "${RELEASE}" --keys "${KEYS}" --output "${SHIPPED}" --app "${PACKAGE}" --packs "${PACKS}" ${deferred} COMMAND_ERROR_IS_FATAL ANY)
if(BOOT)
  file(REMOVE_RECURSE "${BOOT}")
  execute_process(COMMAND "${TOOL}" stage "${RELEASE}" --keys "${KEYS}" --output "${BOOT}" --app-only COMMAND_ERROR_IS_FATAL ANY)
endif()
file(TOUCH "${STAMP}")
