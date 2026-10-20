cmake_minimum_required(VERSION 3.28)
project(rcheevos LANGUAGES C)

include(CMakePackageConfigHelpers)

set(RCHEEVOS_SOURCE_DIR "" CACHE PATH "Pinned rcheevos source directory")
if(NOT EXISTS "${RCHEEVOS_SOURCE_DIR}/include/rc_client_raintegration.h")
  message(FATAL_ERROR "RCHEEVOS_SOURCE_DIR must contain rc_client_raintegration.h")
endif()

set(rc_sources
  src/rapi/rc_api_common.c
  src/rapi/rc_api_info.c
  src/rapi/rc_api_runtime.c
  src/rapi/rc_api_user.c
  src/rc_client.c
  src/rc_client_external.c
  src/rc_client_raintegration.c
  src/rc_compat.c
  src/rc_util.c
  src/rc_version.c
  src/rcheevos/alloc.c
  src/rcheevos/condition.c
  src/rcheevos/condset.c
  src/rcheevos/consoleinfo.c
  src/rcheevos/format.c
  src/rcheevos/lboard.c
  src/rcheevos/memref.c
  src/rcheevos/operand.c
  src/rcheevos/rc_validate.c
  src/rcheevos/richpresence.c
  src/rcheevos/runtime.c
  src/rcheevos/runtime_progress.c
  src/rcheevos/trigger.c
  src/rcheevos/value.c
  src/rhash/cdreader.c
  src/rhash/hash.c
  src/rhash/hash_disc.c
  src/rhash/hash_rom.c
  src/rhash/md5.c
)
list(TRANSFORM rc_sources PREPEND "${RCHEEVOS_SOURCE_DIR}/")

add_library(rcheevos STATIC ${rc_sources})
target_include_directories(rcheevos
  PUBLIC $<BUILD_INTERFACE:${RCHEEVOS_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include/rcheevos>
  PRIVATE "${RCHEEVOS_SOURCE_DIR}/src"
)
target_compile_definitions(rcheevos PUBLIC
  RC_CLIENT_SUPPORTS_HASH RC_CLIENT_SUPPORTS_RAINTEGRATION RC_CLIENT_SUPPORTS_EXTERNAL
  RC_HASH_NO_ZIP RC_HASH_NO_ENCRYPTED RC_STATIC
)

install(TARGETS rcheevos EXPORT rcheevosTargets ARCHIVE DESTINATION lib)
install(DIRECTORY "${RCHEEVOS_SOURCE_DIR}/include/" DESTINATION include/rcheevos)
install(EXPORT rcheevosTargets NAMESPACE rcheevos:: FILE rcheevosTargets.cmake DESTINATION lib/cmake/rcheevos)
configure_package_config_file(rcheevosConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/rcheevosConfig.cmake"
  INSTALL_DESTINATION lib/cmake/rcheevos
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rcheevosConfig.cmake" DESTINATION lib/cmake/rcheevos)
