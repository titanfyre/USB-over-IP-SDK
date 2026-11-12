cmake_minimum_required(VERSION 3.24)
project(USBoverIPSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanUSBoverIPSDK STATIC
  "sdk/SDK/License.cpp"
  "sdk/SDK/Transport.cpp"
  "sdk/SDK/USBOverIP.cpp"
)
add_library(USBoverIPSDK::sdk ALIAS TitanUSBoverIPSDK)
set_target_properties(TitanUSBoverIPSDK PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanUSBoverIPSDK PUBLIC cxx_std_17)
target_compile_definitions(TitanUSBoverIPSDK PUBLIC )
target_compile_definitions(TitanUSBoverIPSDK PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanUSBoverIPSDK PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanUSBoverIPSDK PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanUSBoverIPSDK PUBLIC advapi32 bcrypt cfgmgr32 comctl32 crypt32 gdi32 ncrypt newdev ole32 secur32 setupapi shell32 user32 winhttp wintrust winusb ws2_32)
install(TARGETS TitanUSBoverIPSDK EXPORT USBoverIPSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/usb-over-ip-sdk)
install(EXPORT USBoverIPSDKTargets NAMESPACE USBoverIPSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/USBoverIPSDK)
configure_file("USBoverIPSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/USBoverIPSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/USBoverIPSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/USBoverIPSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/USBoverIPSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/USBoverIPSDK)
