
# Get the architecture-specific part based on the toolchain file
if ("${CMAKE_TOOLCHAIN_FILE}" MATCHES "toolchain-uclibc-linux.cmake")
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
  message(FATAL_ERROR "No shrinked opencv library for ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(OPENCV_VERSION_MAJOR "3")
set(OPENCV_VERSION_MINOR "2")
set(OPENCV_VERSION_PATCH "0")

set(OPENCV_VERSION_MM "${OPENCV_VERSION_MAJOR}.${OPENCV_VERSION_MINOR}")
set(OPENCV_VERSION_STR "${OPENCV_VERSION_MAJOR}.${OPENCV_VERSION_MINOR}.${OPENCV_VERSION_PATCH}")

set(THIRD_PARTY_PATH ${MLIR_SDK_ROOT}/../third_party/opencv_aisdk.tar.gz)
set(DOWNLOAD_PATH "${BUILD_DOWNLOAD_DIR}/opencv-src/${ARCHITECTURE}/opencv_aisdk.tar.gz")

if(NOT IS_DIRECTORY "${BUILD_DOWNLOAD_DIR}/opencv-src")
  # Create the opencv-src directory if it doesn't exist
  file(MAKE_DIRECTORY "${BUILD_DOWNLOAD_DIR}/opencv-src")

  if(EXISTS "${THIRD_PARTY_PATH}")
    message(STATUS "Copying opencv from ${THIRD_PARTY_PATH} to ${DOWNLOAD_PATH}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy ${THIRD_PARTY_PATH} ${DOWNLOAD_PATH}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE result
    )

    if(result EQUAL 0)
      message(STATUS "opencv copied to ${DOWNLOAD_PATH}")
    endif()
  else()
    FetchContent_Declare(
      opencv
      GIT_REPOSITORY https://github.com/sophgo/cvi_opencv.git
      GIT_TAG origin/${ARCHITECTURE}
    )
    FetchContent_MakeAvailable(opencv)
    message("Content downloaded to ${opencv_SOURCE_DIR}")
  endif()

  # Extract the tar.gz file
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${DOWNLOAD_PATH}
    WORKING_DIRECTORY "${BUILD_DOWNLOAD_DIR}/opencv-src"
  )
  message("Content extracted to ${BUILD_DOWNLOAD_DIR}/opencv-src")
endif()

set(OPENCV_ROOT ${BUILD_DOWNLOAD_DIR}/opencv-src)

set(OPENCV_INCLUDES
  ${OPENCV_ROOT}/include/
  ${OPENCV_ROOT}/include/opencv/
)

set(OPENCV_LIBS_IMCODEC ${OPENCV_ROOT}/lib/libopencv_core.so
                        ${OPENCV_ROOT}/lib/libopencv_imgproc.so
                        ${OPENCV_ROOT}/lib/libopencv_imgcodecs.so)

set(OPENCV_LIBS_IMCODEC_STATIC ${OPENCV_ROOT}/lib/libopencv_core.a
                               ${OPENCV_ROOT}/lib/libopencv_imgproc.a
                               ${OPENCV_ROOT}/lib/libopencv_imgcodecs.a)
if (NOT "${CVI_SYSTEM_PROCESSOR}" STREQUAL "RISCV")
  set(OPENCV_LIBS_IMCODEC_STATIC ${OPENCV_LIBS_IMCODEC_STATIC}
                          ${OPENCV_ROOT}/share/OpenCV/3rdparty/lib/libtegra_hal.a)
endif()

set(OPENCV_PATH ${CMAKE_INSTALL_PREFIX}/sample/3rd/opencv)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_core.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_core.so)
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_imgproc.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_imgproc.so)
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_imgcodecs.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_imgcodecs.so)
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_core.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_core.so.${OPENCV_VERSION_MM})
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_imgproc.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_imgproc.so.${OPENCV_VERSION_MM})
install(PROGRAMS ${OPENCV_ROOT}/lib/libopencv_imgcodecs.so.${OPENCV_VERSION_STR} DESTINATION ${OPENCV_PATH}/lib RENAME libopencv_imgcodecs.so.${OPENCV_VERSION_MM})
else()
file(GLOB OPENCV_LIBS "${OPENCV_ROOT}/lib/*so*")
install(FILES ${OPENCV_LIBS} DESTINATION ${OPENCV_PATH}/lib)
endif()
install(FILES ${OPENCV_LIBS_IMCODEC_STATIC} DESTINATION ${OPENCV_PATH}/lib)
install(DIRECTORY ${OPENCV_ROOT}/include/ DESTINATION ${OPENCV_PATH}/include)