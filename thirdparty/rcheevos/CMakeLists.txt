file(
  GLOB_RECURSE
  RCHEEVOS_SOURCES
  CONFIGURE_DEPENDS
  rcheevos/src/*.c
)

list(FILTER RCHEEVOS_SOURCES EXCLUDE REGEX "rcheevos/src/rc_libretro\\.c$")

add_library(rcheevos STATIC ${RCHEEVOS_SOURCES})
add_library(libretro::rcheevos ALIAS rcheevos)

target_include_directories(
  rcheevos
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src"
)

target_compile_definitions(rcheevos PRIVATE RC_DISABLE_LUA=1 RCHEEVOS_URL_SSL)
target_compile_definitions(rcheevos PUBLIC RC_CLIENT_SUPPORTS_HASH)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_compile_definitions(rcheevos PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
