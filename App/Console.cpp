#include "../SDK/USBOverIP.h"
#include <iostream>
#include <string>
#include <stdexcept>
namespace {
std::string Narrow(const wchar_t* text){std::string s;for(;*text;++text){if(*text>127)throw std::runtime_error("IP addresses, pins, secrets and product keys must use ASCII characters.");s.push_back(static_cast<char>(*text));}return s;}
uint16_t Port(const wchar_t* value){std::wstring s=value;size_t used=0;unsigned long p=std::stoul(s,&used);if(used!=s.size()||p>65535||!p)throw std::runtime_error("Port must be 1..65535.");return static_cast<uint16_t>(p);}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==2&&std::wstring(argv[1])==L"--preview"){std::cout<<"USB-over-IP SDK offline preview. No device, driver, network listener or demo was opened.\n";return 0;}
 if(argc==2&&std::wstring(argv[1])==L"devices"){for(const auto& d:UsbIp::Devices())std::wcout<<d.name<<L"\n"<<d.path<<L"\n\n";return 0;}
 if(argc==3&&std::wstring(argv[1])==L"verify"){auto error=UsbIp::Verification(Narrow(argv[2]));if(!error.empty()){std::cerr<<error<<"\n";return 1;}std::cout<<"USB-over-IP unlocked for the current Windows boot.\n";return 0;}
 if(argc==5&&std::wstring(argv[1])==L"export"){UsbIp::Exporter exporter;auto p=exporter.Start(argv[2],Narrow(argv[3]),Port(argv[4]));std::cout<<"Explicit WinUSB export active. Give these pairing details only to the intended peer:\nAddress: "<<p.address<<"\nPort: "<<p.port<<"\nCertificate SHA-256: "<<p.fingerprint<<"\nSession secret: "<<p.secret<<"\nPress Enter to stop.\n";std::cin.get();return 0;}
 if(argc==6&&std::wstring(argv[1])==L"connect"){UsbIp::Client client;client.Connect({Narrow(argv[2]),Narrow(argv[4]),Narrow(argv[5]),Port(argv[3])});std::cout<<"Owned virtual USB device connected through an authenticated encrypted channel.\nPress Enter to disconnect.\n";std::cin.get();return 0;}
 std::cout<<"USB-over-IP SDK - one explicitly selected WinUSB device, encrypted LAN connection.\n\nUSBOverIPCLI.exe devices\nUSBOverIPCLI.exe export \"exact interface path\" 127.0.0.1 28419\nUSBOverIPCLI.exe connect IPv4 port certificate_sha256 session_secret\nUSBOverIPCLI.exe verify product_key\nUSBOverIPCLI.exe --preview\n\nThe default demo lasts one hour per Windows boot. Verification(key) unlocks the same installed driver for this boot.\n";return 0;
}catch(const std::exception& e){std::cerr<<"USB-over-IP: "<<e.what()<<"\n";return 1;}}
