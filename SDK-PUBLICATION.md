# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | usb-over-ip-sdk |
| CPP/conan | published_custom_source_index | [usb-over-ip-sdk](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | published_payload_verified | [USB-over-IP-SDK.Native](https://www.nuget.org/packages/USB-over-IP-SDK.Native/0.1.1) |
| NET/nuget | published_payload_verified | [USBoverIPSDK.SDK](https://www.nuget.org/packages/USBoverIPSDK.SDK/1.0.0) |
| Python/pypi | built_fixture_and_production_verified | usb-over-ip-sdk [Source](USB-over-IP-SDK-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [usb-over-ip-sdk](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [usb-over-ip-sdk](https://crates.io/crates/usb-over-ip-sdk) [Source](USB-over-IP-SDK-Rust-cargo/) |
| JavaScript/npm | stopped_before_submission | usb-over-ip-sdk [Source](USB-over-IP-SDK-JavaScript-npm/) |
| Java/maven | published_install_verified | [usb-over-ip-sdk](https://central.sonatype.com/artifact/io.github.titancsa/usb-over-ip-sdk/1.0.0) [Source](USB-over-IP-SDK-Java-maven/) |
| Java/jitpack | published_install_verified | [usb-over-ip-sdk](https://jitpack.io/#titanfyre/USB-over-IP-SDK/java-sdk-v1.0.0) [Source](USB-over-IP-SDK-Java-maven/) |
| Go/modules | published_install_verified | [usb-over-ip-sdk](https://pkg.go.dev/github.com/titanfyre/USB-over-IP-SDK/USB-over-IP-SDK-Go-modules@v1.0.0) [Source](USB-over-IP-SDK-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/USB-over-IP-SDK/USB-over-IP-SDK-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [USBoverIPSDK](https://www.powershellgallery.com/packages/USBoverIPSDK/1.0.0) [Source](USB-over-IP-SDK-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | usb_over_ip_sdk [Source](USB-over-IP-SDK-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [usb-over-ip-sdk](https://rubygems.org/gems/usb-over-ip-sdk) [Source](USB-over-IP-SDK-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/usb-over-ip-sdk](https://packagist.org/packages/titancsa/usb-over-ip-sdk) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
