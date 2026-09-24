cmake_minimum_required(VERSION 3.16)
project(whwd VERSION 0.1.0 LANGUAGES C RC)

if(NOT WIN32)
  message(FATAL_ERROR "whwd targets Windows platforms only")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(WHWD_TARGET_XP "Aim for Windows XP compatibility (x86, v141_xp toolset)" OFF)

if(NOT DEFINED WHWD_CI_BUILD)
  set(WHWD_CI_BUILD "0")
endif()
if(NOT DEFINED WHWD_CI_SHA)
  set(WHWD_CI_SHA "00000000")
endif()
string(SUBSTRING "${WHWD_CI_SHA}" 0 8 WHWD_SHA8)
set(WHWD_VERSION_STRING "0.${WHWD_CI_BUILD}-${WHWD_SHA8}")
set(PROJECT_VERSION "${WHWD_VERSION_STRING}")

string(TIMESTAMP WHWD_BUILD_DS "%Y-%m-%d %H:%M:%S")
string(TIMESTAMP WHWD_BUILD_TZ "%z")
string(SUBSTRING "${WHWD_BUILD_TZ}" 0 3 WHWD_BUILD_TZ_SIGN)
string(SUBSTRING "${WHWD_BUILD_TZ}" 3 2 WHWD_BUILD_TZ_MIN)
set(WHWD_BUILD_TIME_STRING "${WHWD_BUILD_DS}${WHWD_BUILD_TZ_SIGN}:${WHWD_BUILD_TZ_MIN}")

if(WHWD_TARGET_XP AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "WHWD_TARGET_XP requires a 32-bit x86 build")
endif()

if(WHWD_TARGET_XP)
  set(CMAKE_GENERATOR_TOOLSET v141_xp)
else()
  set(WHWD_WINVER 0x0601)
endif()

add_library(whwd_core STATIC
  src/core/device.c
  src/core/feature.c
  src/core/version.c
  src/core/source.c
  src/core/sources/wu_source.c
  src/core/sources/realtek_source.c
  src/core/sources/intel_source.c
)

target_include_directories(whwd_core PUBLIC src/core/include)

if(MSVC)
  target_compile_options(whwd_core PRIVATE /W4)
else()
  target_compile_options(whwd_core PRIVATE -Wall -Wextra)
endif()

if(NOT WHWD_TARGET_XP)
  target_compile_definitions(whwd_core PUBLIC
    _WIN32_WINNT=${WHWD_WINVER}
    WINVER=${WHWD_WINVER}
    UNICODE
    _UNICODE
  )
else()
  target_compile_definitions(whwd_core PUBLIC
    _WIN32_WINNT=0x0501
    WINVER=0x0501
    UNICODE
    _UNICODE
  )
endif()

target_link_libraries(whwd_core PUBLIC
  setupapi
  advapi32
  ole32
  oleaut32
  shell32
  version
)

configure_file(src/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
configure_file(src/gui/gui.rc.in ${CMAKE_CURRENT_BINARY_DIR}/gui.rc @ONLY)
configure_file(src/whwd_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/whwd_version.h @ONLY)

add_executable(whwd src/cli/main.c ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

target_include_directories(whwd PRIVATE
  src/core/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

if(MSVC)
  target_compile_options(whwd PRIVATE /W4)
endif()

target_link_libraries(whwd PRIVATE whwd_core)

if(MSVC)
  target_link_options(whwd PRIVATE
    "/MANIFEST:NO"
    "/SUBSYSTEM:CONSOLE,6.01"
  )
endif()

add_executable(whwd-gui WIN32 src/gui/main.c ${CMAKE_CURRENT_BINARY_DIR}/gui.rc)

target_include_directories(whwd-gui PRIVATE
  src/core/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

if(NOT WHWD_TARGET_XP)
  target_compile_definitions(whwd-gui PRIVATE
    _WIN32_WINNT=${WHWD_WINVER}
    WINVER=${WHWD_WINVER}
    UNICODE
    _UNICODE
  )
else()
  target_compile_definitions(whwd-gui PRIVATE
    _WIN32_WINNT=0x0600
    WINVER=0x0600
    UNICODE
    _UNICODE
  )
endif()

if(MSVC)
  target_compile_options(whwd-gui PRIVATE /W4)
endif()

target_link_libraries(whwd-gui PRIVATE whwd_core comctl32)

if(MSVC)
  target_link_options(whwd-gui PRIVATE
    "/MANIFEST:NO"
    "/SUBSYSTEM:WINDOWS,6.01"
  )
endif()

install(TARGETS whwd whwd-gui RUNTIME DESTINATION .)