set(symbol_map ${CMAKE_CURRENT_SOURCE_DIR}/symbols.map)

add_library(mirclientplatformandroidobjects OBJECT
  buffer.cpp
  android_client_buffer_factory.cpp
  gralloc_registrar.cpp
  android_client_platform.cpp
  client_platform_factory.cpp
  egl_native_surface_interpreter.cpp
  android_native_display_container.cpp
)

target_compile_definitions(mirclientplatformandroidobjects PRIVATE ANDROID)
target_include_directories(mirclientplatformandroidobjects 
  PRIVATE ${client_common_include_dirs}
  SYSTEM PRIVATE ${ANDROID_HEADERS_INCLUDE_DIRS}
  )
add_library(mirclientplatformandroid MODULE
  $<TARGET_OBJECTS:mirclientplatformandroidobjects>
)

set_target_properties(
  mirclientplatformandroid PROPERTIES
  OUTPUT_NAME android2
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/client-modules
  PREFIX ""
  SUFFIX ".so.${MIR_CLIENT_PLATFORM_ABI}"
  LINK_FLAGS "-Wl,--version-script,${symbol_map}"
  LINK_DEPENDS ${symbol_map}
)

target_link_libraries(mirclientplatformandroid
  client_platform_common
  mirsharedandroid-static
  ${MIRCORE_LDFLAGS}
  ${MIRPLATFORM_LDFLAGS}
  ${MIRCLIENT_LDFLAGS}
  dl
  ${GRALLOC_LIBRARIES}
  ${HYBRIS_EGL_PLATFORM_LIBRARIES}
  ${EGL_LDFLAGS} ${EGL_LIBRARIES}
)

install(TARGETS mirclientplatformandroid LIBRARY DESTINATION ${MIR_CLIENT_PLATFORM_PATH})
