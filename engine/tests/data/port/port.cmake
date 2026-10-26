# The smallest port of the engine, which "haylen test --port" builds with the engine and runs on every desktop. Its platform plays apps without a window or a GPU, through a device of the API "port" that checks every call against the contract of the GPU device, and its shader tool gives each shader package the code of that device.
haylen_port(
  PLATFORM test-port
  RUNTIME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/TestPortHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortRuntime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortServices.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestPortWindow.cpp"
  DEVICE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PortDevice.cpp"
    "${HAYLEN_ENGINE_DIR}/src/gpu/NullDevice.cpp"
  SHADER_LANGUAGES spirv
  SHADER_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/port-shaders.cmake"
)
