add_executable(
  desktop-ui
  desktop-ui.cpp
  resource/resource.cpp
  input/input.cpp
  emulator/emulator.cpp
  game-browser/game-browser.cpp
  program/program.cpp
  presentation/presentation.cpp
  retroachievements/retroachievements.cpp
  retroachievements/debug.cpp
  retroachievements/platform/platform.cpp
  retroachievements/platform/adapter.cpp
  retroachievements/platform/adapter_registry.cpp
  retroachievements/platform/fds.cpp
  retroachievements/platform/hash_chd.cpp
  retroachievements/platform/adapters/nintendo.cpp
  retroachievements/platform/adapters/sega.cpp
  retroachievements/platform/adapters/sony.cpp
  retroachievements/platform/adapters/nec.cpp
  retroachievements/platform/adapters/snk.cpp
  retroachievements/platform/adapters/misc.cpp
  retroachievements/http.cpp
  retroachievements/client.cpp
  retroachievements/auth.cpp
  retroachievements/images.cpp
  retroachievements/state_io.cpp
  settings/settings.cpp
  tools/tools.cpp
)

target_sources(desktop-ui PRIVATE cmake/sources.cmake cmake/os-macos.cmake cmake/os-linux.cmake cmake/os-windows.cmake)

include(cmake/sources.cmake)

get_target_property(desktop-ui_SOURCES desktop-ui SOURCES)

set_source_files_properties(desktop-ui ${desktop-ui_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

set_source_files_properties(
  desktop-ui
  desktop-ui.cpp
  resource/resource.cpp
  input/input.cpp
  emulator/emulator.cpp
  game-browser/game-browser.cpp
  program/program.cpp
  presentation/presentation.cpp
  retroachievements/retroachievements.cpp
  retroachievements/debug.cpp
  retroachievements/platform/platform.cpp
  retroachievements/platform/adapter.cpp
  retroachievements/platform/adapter_registry.cpp
  retroachievements/platform/fds.cpp
  retroachievements/platform/hash_chd.cpp
  retroachievements/platform/adapters/nintendo.cpp
  retroachievements/platform/adapters/sega.cpp
  retroachievements/platform/adapters/sony.cpp
  retroachievements/platform/adapters/nec.cpp
  retroachievements/platform/adapters/snk.cpp
  retroachievements/platform/adapters/misc.cpp
  retroachievements/http.cpp
  retroachievements/client.cpp
  retroachievements/auth.cpp
  retroachievements/images.cpp
  retroachievements/state_io.cpp
  settings/settings.cpp
  tools/tools.cpp
  PROPERTIES HEADER_FILE_ONLY FALSE
)

if(ARES_ENABLE_LIBRASHADER)
  find_package(slang_shaders)
endif()

if(OS_WINDOWS)
  include(cmake/os-windows.cmake)
elseif(OS_MACOS)
  include(cmake/os-macos.cmake)
elseif(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  include(cmake/os-linux.cmake)
endif()

target_include_directories(desktop-ui PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(desktop-ui PRIVATE ARES_ENABLE_RCHEEVOS=$<BOOL:${ARES_ENABLE_RCHEEVOS}>)
target_link_libraries(
  desktop-ui
  PRIVATE ares::ruby ares::hiro ares::ares ares::mia ares::nall
)

if(ARES_ENABLE_RCHEEVOS)
  target_link_libraries(desktop-ui PRIVATE libretro::rcheevos)

  if(TARGET ares::curl)
    target_link_libraries(desktop-ui PRIVATE ares::curl)
  elseif(ARES_CURL_LINK_LIBRARIES)
    target_link_libraries(desktop-ui PRIVATE ${ARES_CURL_LINK_LIBRARIES})
  endif()

  if(ARES_CURL_INCLUDE_DIRS)
    set_source_files_properties(
      retroachievements/http.cpp
      retroachievements/images.cpp
      PROPERTIES INCLUDE_DIRECTORIES "${ARES_CURL_INCLUDE_DIRS}"
    )
  endif()
endif()
# if(ARES_ENABLE_RCHEEVOS)
#   target_link_libraries(desktop-ui PRIVATE CURL::libcurl libretro::rcheevos)
# endif()

if(ARES_ENABLE_CHD)
  target_link_libraries(desktop-ui PRIVATE chdr-static)
endif()

set_target_properties(desktop-ui PROPERTIES OUTPUT_NAME ares)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${desktop-ui_SOURCES})

add_sourcery_command(desktop-ui resource)

ares_configure_executable(desktop-ui)
