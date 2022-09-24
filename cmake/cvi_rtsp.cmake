include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

get_filename_component(_deps "../_deps" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
set(FETCHCONTENT_BASE_DIR ${_deps})

if ("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-uclibc-linux.cmake")
  set(RTSP_SDK_VER "uclibc")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-aarch64-linux.cmake")
  set(RTSP_SDK_VER "64bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-gnueabihf-linux.cmake")
  set(RTSP_SDK_VER "32bit")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-riscv64-linux.cmake")
  set(RTSP_SDK_VER "glibc_riscv64")
elseif("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-riscv64-musl.cmake")
  set(RTSP_SDK_VER "musl_riscv64")
else()
  message(FATAL_ERROR "Unrecognized toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT EXISTS "${FETCHCONTENT_BASE_DIR}/cvi_rtsp/src/cvi_rtsp/src/libcvi_rtsp.so")
  if(EXISTS $ENV{TOP_DIR}/cvi_rtsp)
    ExternalProject_Add(cvi_rtsp
      URL "file://$ENV{TOP_DIR}/cvi_rtsp"
      PREFIX ${FETCHCONTENT_BASE_DIR}/cvi_rtsp
      BUILD_COMMAND CROSS_COMPILE=${TC_PATH}${CROSS_COMPILE} SDK_VER=${RTSP_SDK_VER} ./build.sh
      CONFIGURE_COMMAND ""
      INSTALL_COMMAND ""
      BUILD_IN_SOURCE true
      BUILD_BYPRODUCTS <SOURCE_DIR>/src/libcvi_rtsp.so
    )
  else()
    ExternalProject_Add(cvi_rtsp
      GIT_REPOSITORY ssh://10.240.0.84:29418/cvi_rtsp
      PREFIX ${FETCHCONTENT_BASE_DIR}/cvi_rtsp
      BUILD_COMMAND CROSS_COMPILE=${TC_PATH}${CROSS_COMPILE} SDK_VER=${RTSP_SDK_VER} ./build.sh
      CONFIGURE_COMMAND ""
      INSTALL_COMMAND ""
      BUILD_IN_SOURCE true
      BUILD_BYPRODUCTS <SOURCE_DIR>/src/libcvi_rtsp.so
    )
  endif()
  ExternalProject_Get_property(cvi_rtsp SOURCE_DIR)
  message("Content downloaded to ${SOURCE_DIR}")
else()
  set(SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/cvi_rtsp/src/cvi_rtsp)
endif()

set(cvi_rtsp_LIBPATH ${SOURCE_DIR}/src/libcvi_rtsp.so)
set(cvi_rtsp_INCLUDE ${SOURCE_DIR}/include/cvi_rtsp)

install(FILES ${cvi_rtsp_LIBPATH} DESTINATION lib)
install(DIRECTORY ${cvi_rtsp_INCLUDE} DESTINATION include)
