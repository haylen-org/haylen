# The smallest port of the engine, which "haylen test --port" builds with the engine and runs on every desktop. Its platform plays apps without a window or a GPU, through a device of the API "port" that checks every call against the contract of the GPU device, its shader tool gives each shader package the code of that device, and it builds its apps and its SDK like a port of a real platform.
haylen_port(
  CONTRACT 2
  PLATFORM test-port
  CAPABILITIES dynamicLibraries processes
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
