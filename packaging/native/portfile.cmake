vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/USB-over-IP-SDK/releases/download/sdk-v0.1.1/usb-over-ip-sdk-sdk-0.1.1.tar.gz"
    FILENAME "usb-over-ip-sdk-sdk-0.1.1.tar.gz"
    SHA512 68dcaadbbe15d7fbd084a4554fee37f351cbd5669962ce135264327c44d9b10bee624d6b8b10e341d8b2c1d876b18989fe51c07a2a31b650ef8457e359fb7faa
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME USBoverIPSDK CONFIG_PATH lib/cmake/USBoverIPSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
