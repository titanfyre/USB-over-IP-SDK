#include "../Installer/UI/Setup.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <objbase.h>
#include <cstring>
int main(){
 wchar_t temporary[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,temporary))return 1;GUID guid{};if(FAILED(CoCreateGuid(&guid)))return 1;wchar_t name[40]{};StringFromGUID2(guid,name,40);auto directory=std::filesystem::path(temporary)/(std::wstring(L"UsbIpIdentity-")+name);std::filesystem::create_directory(directory);auto path=directory/L"identity.inf";
 struct Cleanup{std::filesystem::path path,directory;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);std::filesystem::remove(directory,error);}} cleanup{path,directory};
 const std::string valid=R"INF([Version]
Signature="$Windows NT$"
Class=USB
ClassGuid={36FC9E60-C465-11CF-8056-444553540000}
Provider=%Provider%
CatalogFile=TitanUSBOverIP.cat
DriverVer=10/26/2026,0.1.0.0
[Manufacturer]
%Provider%=Devices,NTamd64.10.0...19041
[Devices.NTamd64.10.0...19041]
%Device%=Install,ROOT\TitanUSBOverIP
[Install.NT]
CopyFiles=DriverFiles
[Install.NT.Services]
AddService=TitanUSBOverIP,0x00000002,Service
[Service]
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%13%\TitanUSBOverIP.sys
[DestinationDirs]
DriverFiles=13
[SourceDisksNames]
1=%Device%
[SourceDisksFiles]
TitanUSBOverIP.sys=1
[DriverFiles]
TitanUSBOverIP.sys
[Strings]
Provider="Titanfyre"
Device="Titan USB-over-IP Virtual Controller"
)INF";
 auto write=[&](const std::string& text){std::ofstream stream(path,std::ios::binary);stream<<text;stream.close();return !stream.fail();};
 if(!write(valid)||!Setup::PackageMatches(path.wstring()))return 1;unsigned checks=1;
 for(const auto& change:{std::pair{"36FC9E60","4D36E97D"},std::pair{"Provider=\"Titanfyre\"","Provider=\"OtherVendor\""},std::pair{"CatalogFile=TitanUSBOverIP.cat","CatalogFile=Other.cat"},std::pair{"Install,ROOT\\TitanUSBOverIP","Install,ROOT\\OtherDevice"}}){auto text=valid;auto offset=text.find(change.first);if(offset==text.npos)return 1;text.replace(offset,std::strlen(change.first),change.second);if(!write(text)||Setup::PackageMatches(path.wstring()))return 1;++checks;}
 if(Setup::CatalogTrusted(path.wstring())||Setup::CatalogTrusted((directory/L"missing.cat").wstring()))return 1;checks+=2;
 std::cout<<checks<<" read-only installer identity/signature refusal checks passed; no installation or USB association was performed.\n";
}
