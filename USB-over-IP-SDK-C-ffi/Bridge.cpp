#include "SDK/Transport.h"
#include "SDK/USBOverIP.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<UsbIp::Exporter> exporter;std::unique_ptr<UsbIp::Client> client;
UsbIp::Exporter& Host(){if(!exporter)exporter=std::make_unique<UsbIp::Exporter>();return *exporter;}
UsbIp::Client& Viewer(){if(!client)client=std::make_unique<UsbIp::Client>();return *client;}
Json Status(const UsbIp::Status& s){return {{"active",s.active},{"connected",s.connected},{"requests",std::to_string(s.requests)},{"completed",std::to_string(s.completed)},{"failed",std::to_string(s.failed)},{"detail",s.detail}};}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto pairing=a.at("pairing");auto fingerprint=pairing.at("fingerprint").get<std::string>();auto secret=pairing.at("secret").get<std::string>();return {{"valid",TitanTransport::IsFingerprint(fingerprint) && TitanTransport::IsSecret(secret)},{"scope","TLS fingerprint and secret format; native Connect additionally checks address, port and authentication"}};}
if(op=="devices"){Json devices=Json::array();for(const auto& d:UsbIp::Devices())devices.push_back({{"name",Utf8(d.name)},{"path",Utf8(d.path)}});return devices;}
if(op=="verification"){CheckMessage(UsbIp::Verification(a.at("key").get<std::string>()));return Json::object();}
if(op=="export"){auto p=Host().Start(Wide(a.at("device_path").get<std::string>()),a.value("bind",std::string("127.0.0.1")),static_cast<std::uint16_t>(Number(a,"port",0,65535)));return {{"address",p.address},{"port",p.port},{"fingerprint",p.fingerprint},{"secret",p.secret}};}
if(op=="connect"){UsbIp::Pairing p;p.address=a.at("address").get<std::string>();p.port=static_cast<std::uint16_t>(Number(a,"port",0,65535));p.fingerprint=a.at("fingerprint").get<std::string>();p.secret=a.at("secret").get<std::string>();Viewer().Connect(p);return Json::object();}
if(op=="export_status")return Status(Host().GetStatus());
if(op=="client_status")return Status(Viewer().GetStatus());
if(op=="stop_export"){if(exporter)exporter->Stop();return Json::object();}
if(op=="stop_client"){if(client)client->Stop();return Json::object();}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned usb_over_ip_sdk_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned usb_over_ip_sdk_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* usb_over_ip_sdk_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void usb_over_ip_sdk_json_free(char* response)noexcept{delete[] response;}
