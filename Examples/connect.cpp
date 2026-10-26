#include "../SDK/USBOverIP.h"
#include <iostream>
int main(){
 std::cout<<"Connect one exported WinUSB device using pairing details from its owner.\n";
 UsbIp::Pairing peer;
 std::cout<<"Exporter IPv4: ";std::cin>>peer.address;
 std::cout<<"Port: ";std::cin>>peer.port;
 std::cout<<"Certificate SHA-256: ";std::cin>>peer.fingerprint;
 std::cout<<"Session secret: ";std::cin>>peer.secret;
 try{
  UsbIp::Client device;
  device.Connect(peer);
  std::cout<<"Connected. Press Enter to disconnect.\n";
  std::cin.ignore();std::cin.get();
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
