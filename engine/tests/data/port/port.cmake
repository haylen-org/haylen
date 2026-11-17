# The smallest port of the engine, which "haylen test --port" builds with the engine and runs on every desktop. It takes the drivers of Varn that the desktop has, and an external driver of processes of its own, which starts nothing. Its platform plays apps without a window or a GPU, through a device of the API "port" that checks every call against the contract of the GPU device, its shader tool gives each shader package the code of that device, and it builds its apps and its SDK like a port of a real platform.
# The test port runs on the desktop that builds it, so its system code is that of Windows there and that of POSIX on the other desktops.
if(WIN32)
  set(test_port_system_api windows)
else()
  set(test_port_system_api posix)
endif()

haylen_port(
  CONTRACT 2
  PLATFORM test-port
  SYSTEM_API ${test_port_system_api}
  CAPABILITIES dynamicLibraries
  VARN_DRIVERS SOCKET=POCO HTTP_CLIENT=POCO HTTP_SERVER=POCO CRYPTO=OPENSSL FFI=LIBFFI LOG=SPDLOG TLS=ON PROCESS=EXTERNAL
  VARN_DRIVER_SOURCES "PROCESS=${CMAKE_CURRENT_LIST_DIR}/TestPortProcessRunner.cpp"
  RUNTIME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/TestPortHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortRuntime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortServices.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortWindow.cpp"
  DEVICE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PortDevice.cpp"
  SHADER_LANGUAGES spirv
  SHADER_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/port-shaders.cmake"
  HOST_TOOLS "${CMAKE_CURRENT_LIST_DIR}/port-shaders.cmake"
  APP_FUNCTION haylen_test_port_add_app
  APP_MODULE "${CMAKE_CURRENT_LIST_DIR}/port-app.cmake"
  RELEASE_PROFILE test-port
  PLUGIN_PART test-port
  SDK ON
)
