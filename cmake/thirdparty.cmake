project(thirdparty_fetchcontent)

# Get the architecture-specific part based on the toolchain file
if ("${CMAKE_TOOLCHAIN_FILE}" MATCHES "arm-cvitek-linux-uclibcgnueabihf.cmake")
  set(ARCHITECTURE "uclibc")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "arm-linux-gnueabihf.cmake")
  set(ARCHITECTURE "32bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "aarch64-linux-gnu.cmake")
  set(ARCHITECTURE "64bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "aarch64-none-linux-gnu.cmake")
  set(ARCHITECTURE "64bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "aarch64-buildroot-linux-gnu.cmake")
  set(ARCHITECTURE "64bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "riscv64-unknown-linux-gnu.cmake")
  set(ARCHITECTURE "glibc_riscv64")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "riscv64-unknown-linux-musl.cmake")
  set(ARCHITECTURE "musl_riscv64")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "arm-none-linux-musleabihf.cmake")
  set(ARCHITECTURE "musl_arm")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-uclibc-linux.cmake")
  set(ARCHITECTURE "uclibc")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-gnueabihf-linux.cmake")
  set(ARCHITECTURE "32bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-aarch64-linux.cmake")
  set(ARCHITECTURE "64bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-riscv64-linux.cmake")
  set(ARCHITECTURE "glibc_riscv64")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-riscv64-musl.cmake")
  set(ARCHITECTURE "musl_riscv64")
else()
  message(FATAL_ERROR "No shrinked 3rd party library for ${CMAKE_TOOLCHAIN_FILE}")
endif()

#if (IS_LOCAL)
#  set(EIGEN_URL ${3RD_PARTY_URL_PREFIX}${ARCHITECTURE}/eigen.tar.gz)
#else()
#  set(EIGEN_URL ${TOP_DIR}/oss/oss_release_tarball/${ARCHITECTURE}/eigen.tar.gz)
#endif()

if (NOT IS_DIRECTORY  "${BUILD_DOWNLOAD_DIR}/libeigen-src")
  FetchContent_Declare(
    libeigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG bea7f7c582ab3c11a1e2773d6636aa87390a67a7 # nightly-20250926
  )
  FetchContent_MakeAvailable(libeigen)
  message("Content downloaded to ${libeigen_SOURCE_DIR}")
endif()
#include_directories(${BUILD_DOWNLOAD_DIR}/libeigen-src/include/eigen3)
include_directories(${BUILD_DOWNLOAD_DIR}/libeigen-src)

if (IS_LOCAL)
  set(GOOGLETEST_URL ${3RD_PARTY_URL_PREFIX}${ARCHITECTURE}/googletest.tar.gz)
else()
  set(GOOGLETEST_URL ${TOP_DIR}/oss/oss_release_tarball/${ARCHITECTURE}/googletest.tar.gz)
endif()

set(BUILD_GMOCK OFF CACHE BOOL "Build GMOCK")
set(INSTALL_GTEST OFF CACHE BOOL "Install GMOCK")
if (NOT IS_DIRECTORY "${BUILD_DOWNLOAD_DIR}/googletest-src")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG  e2239ee6043f73722e7aa812a459f54a28552929 # release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
  message("Content downloaded to ${googletest_SOURCE_DIR}")
else()
  project(googletest)
  add_subdirectory(${BUILD_DOWNLOAD_DIR}/googletest-src/)
endif()
include_directories(${BUILD_DOWNLOAD_DIR}/googletest-src/googletest/include/gtest)

if (IS_LOCAL)
  set(NLOHMANNJSON_URL ${3RD_PARTY_URL_PREFIX}${ARCHITECTURE}/nlohmannjson.tar.gz)
else()
  set(NLOHMANNJSON_URL ${TOP_DIR}/oss/oss_release_tarball/${ARCHITECTURE}/nlohmannjson.tar.gz)
endif()

if(NOT IS_DIRECTORY "${BUILD_DOWNLOAD_DIR}/nlohmannjson-src")
  FetchContent_Declare(
    nlohmannjson
    URL ${NLOHMANNJSON_URL}
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )
  FetchContent_MakeAvailable(nlohmannjson)
  message("Content downloaded to ${nlohmannjson_SOURCE_DIR}")
endif()
include_directories(${BUILD_DOWNLOAD_DIR}/nlohmannjson-src)

if (IS_LOCAL)
  set(STB_URL ${3RD_PARTY_URL_PREFIX}${ARCHITECTURE}/stb.tar.gz)
else()
  set(STB_URL ${TOP_DIR}/oss/oss_release_tarball/${ARCHITECTURE}/stb.tar.gz)
endif()


if(NOT IS_DIRECTORY "${BUILD_DOWNLOAD_DIR}/stb-src")
  FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG origin/master
  )
  FetchContent_MakeAvailable(stb)
  message("Content downloaded to ${stb_SOURCE_DIR}")
endif()
set(stb_SOURCE_DIR ${BUILD_DOWNLOAD_DIR}/stb-src)
include_directories(${stb_SOURCE_DIR})

install(DIRECTORY  ${stb_SOURCE_DIR}/ DESTINATION sample/3rd/stb/include
    FILES_MATCHING PATTERN "*.h"
    PATTERN ".git" EXCLUDE
    PATTERN ".github" EXCLUDE
    PATTERN "data" EXCLUDE
    PATTERN "deprecated" EXCLUDE
    PATTERN "docs" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "tools" EXCLUDE)
