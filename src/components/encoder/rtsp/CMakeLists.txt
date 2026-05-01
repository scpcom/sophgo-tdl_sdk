get_filename_component(FILE_NAME ${CMAKE_TOOLCHAIN_FILE} NAME_WE)

if ("${FILE_NAME}" MATCHES "uclibc")
  set(RTSP_SDK_VER "uclibc")
elseif("${FILE_NAME}" MATCHES "aarch64")
  set(RTSP_SDK_VER "64bit")
elseif("${FILE_NAME}" MATCHES "gnueabihf")
  set(RTSP_SDK_VER "32bit")
elseif("${FILE_NAME}" MATCHES "riscv64-linux")
  set(RTSP_SDK_VER "glibc_riscv64")
elseif("${FILE_NAME}" MATCHES "riscv64-musl")
  set(RTSP_SDK_VER "musl_riscv64")
elseif("${FILE_NAME}" MATCHES "riscv64-linux-gnu")
  set(RTSP_SDK_VER "glibc_riscv64")
elseif("${FILE_NAME}" MATCHES "riscv64-unknown-linux-gnu")
  set(RTSP_SDK_VER "glibc_riscv64")
elseif("${FILE_NAME}" MATCHES "riscv64-unknown-linux-musl")
  set(RTSP_SDK_VER "musl_riscv64")
else()
  set(RTSP_SDK_VER "unknown")
  message(WARNING "Unrecognized toolchain file: ${CMAKE_TOOLCHAIN_FILE}. Defaulting to RTSP_SDK_VER=${RTSP_SDK_VER}")
endif()

project(rtsp)

if(EXISTS $ENV{TOP_DIR}/cvi_rtsp)
  set(PROJECT_URL "$ENV{TOP_DIR}/cvi_rtsp")
else()
  set(PROJECT_URL "https://github.com/sophgo/cvi_rtsp.git")
endif()

message(STATUS "PROJECT_URL for cvi_rtsp is: ${PROJECT_URL}")

set(SOURCE_DIR ${BUILD_DOWNLOAD_DIR}/cvi_rtsp)

if(NOT EXISTS "$ENV{TOP_DIR}/cvi_rtsp/src/libcvi_rtsp.so")
  if(NOT EXISTS ${SOURCE_DIR}/src/libcvi_rtsp.so)
    if(EXISTS "${PROJECT_URL}")
      message(STATUS "Copying cvi_rtsp from ${PROJECT_URL} to ${BUILD_DOWNLOAD_DIR}")
      execute_process(
        COMMAND cp -r ${PROJECT_URL} ${BUILD_DOWNLOAD_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE result
      )

      if(result EQUAL 0)
        message(STATUS "cvi_rtsp copied to ${BUILD_DOWNLOAD_DIR}")
        if(EXISTS "${SOURCE_DIR}/build.sh")
          message(STATUS "build.sh exists and is executable")
        endif()
        add_custom_command(
          OUTPUT ${SOURCE_DIR}/src/libcvi_rtsp.so ${SOURCE_DIR}/src/libcvi_rtsp.a
          COMMAND CROSS_COMPILE=${TC_PATH}${CROSS_COMPILE} LIVE555_DIR=${MLIR_SDK_ROOT} SDK_VER=${RTSP_SDK_VER} ./build.sh
          WORKING_DIRECTORY ${BUILD_DOWNLOAD_DIR}/cvi_rtsp
          VERBATIM
          COMMENT "Building libcvi_rtsp from copied files"
        )

        add_custom_target(build_cvi_rtsp ALL DEPENDS ${SOURCE_DIR}/src/libcvi_rtsp.so)
        add_custom_target(build_cvi_rtsp-static ALL DEPENDS ${SOURCE_DIR}/src/libcvi_rtsp.a)
      endif()

    elseif(PROJECT_URL MATCHES "^https://")
      ExternalProject_Add(cvi_rtsp
        GIT_REPOSITORY ${PROJECT_URL}
        PREFIX ${BUILD_DOWNLOAD_DIR}/cvi_rtsp
        BUILD_COMMAND CROSS_COMPILE=${TC_PATH}${CROSS_COMPILE} LIVE555_DIR=${MLIR_SDK_ROOT} SDK_VER=${RTSP_SDK_VER} ./build.sh
        CONFIGURE_COMMAND ""
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE true
        BUILD_BYPRODUCTS <SOURCE_DIR>/src/libcvi_rtsp.so
      )
      ExternalProject_Get_property(cvi_rtsp SOURCE_DIR)
      message("Content downloaded to ${SOURCE_DIR}")
    endif()

  endif()
else()
  set(SOURCE_DIR $ENV{TOP_DIR}/cvi_rtsp)
endif()

message(STATUS "rtsp SOURCE_DIR: ${SOURCE_DIR}")

if (${CVI_PLATFORM} STREQUAL "CV181X" OR ${CVI_PLATFORM} STREQUAL "CV184X")
    #set(SOURCE_DIR $ENV{TOP_DIR}/cvi_rtsp/install)
    set(CVI_RTSP_LIBPATH ${SOURCE_DIR}/src/libcvi_rtsp.so)
    if(${CVI_PLATFORM} STREQUAL "CV181X")
        set(OTHER_LIB_DIR ${MLIR_SDK_ROOT}/lib)
    else()
        set(OTHER_LIB_DIR ${TOP_DIR}/cvi_rtsp/prebuilt/lib)
    endif()
    set(CVI_RTSP_LIBPATH_STATIC 
        ${OTHER_LIB_DIR}/libBasicUsageEnvironment.a
        ${OTHER_LIB_DIR}/libgroupsock.a
        ${OTHER_LIB_DIR}/libUsageEnvironment.a
        ${OTHER_LIB_DIR}/libliveMedia.a
        ${SOURCE_DIR}/src/libcvi_rtsp.a
    )
    set(CVI_RTSP_INCLUDE ${SOURCE_DIR}/include/cvi_rtsp)
    set(RTSP_INCLUDE ${CVI_RTSP_INCLUDE} PARENT_SCOPE)
    set(RTSP_LIBS_SHARED ${CVI_RTSP_LIBPATH} PARENT_SCOPE)
    set(RTSP_LIBS_STATIC ${CVI_RTSP_LIBPATH_STATIC} PARENT_SCOPE)
endif()

message(STATUS "CVI_RTSP_LIBPATH: ${CVI_RTSP_LIBPATH}")
message(STATUS "CVI_RTSP_INCLUDE: ${CVI_RTSP_INCLUDE}")

# Add link directories for ISP and sensor libraries
link_directories(
    ${MLIR_LIBS}
    ${MIDDLEWARE_LIBS} 
    ${OPENCV_LIBRARIES}
)

include_directories(
    ${MLIR_INCLUDES}
    ${MIDDLEWARE_INCLUDES}
    ${CVI_RTSP_INCLUDE}
    ${REPO_DIR}/include/
    ${REPO_DIR}/include/framework
    ${REPO_DIR}/include/components
)

# Install headers
install(FILES ${REPO_DIR}/include/components/encoder/rtsp/rtsp.hpp
    DESTINATION sample/utils/include/
)

file(GLOB_RECURSE SRC_FRAMWORK_FILES_CUR ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

message(STATUS "SRC_RTSP_FILES_CUR: ${SRC_FRAMWORK_FILES_CUR}")
add_library(${PROJECT_NAME} OBJECT ${SRC_FRAMWORK_FILES_CUR})