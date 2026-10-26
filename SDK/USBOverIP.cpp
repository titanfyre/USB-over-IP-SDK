#include "Transport.h" // Winsock must precede Windows headers.
#include "USBOverIP.h"
#include "Wire.h"
#include "DescriptorValidation.h"
#include "License.h"
#include <winusb.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <utility>
#pragma comment(lib,"winusb.lib")
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"cfgmgr32.lib")
#pragma comment(lib,"ole32.lib")
namespace UsbIp {
namespace {
using Bytes=std::vector<uint8_t>;
using Clock=std::chrono::steady_clock;
[[noreturn]] void Fail(const char* stage,DWORD error=GetLastError()){
 wchar_t* message=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<LPWSTR>(&message),0,nullptr);
 std::string detail;
 if(message){int n=WideCharToMultiByte(CP_UTF8,0,message,-1,nullptr,0,nullptr,nullptr);if(n>1){detail.resize(size_t(n));WideCharToMultiByte(CP_UTF8,0,message,-1,detail.data(),n,nullptr,nullptr);detail.pop_back();}LocalFree(message);}
 throw std::runtime_error(std::string(stage)+" failed (Windows error "+std::to_string(error)+"). "+detail);
}
void RequireLicense(bool begin){TS_STATUS s{};auto error=begin?TitanSdk::Begin(TITAN_SDK_USB_OVER_IP,s):TitanSdk::Status(TITAN_SDK_USB_OVER_IP,s);if(!error.empty())throw std::runtime_error(error);if(s.mode!=TITAN_SDK_MODE_DEMO&&s.mode!=TITAN_SDK_MODE_FULL)throw std::runtime_error("The USB-over-IP demo has expired. Call Verification(key) to unlock this product for the current Windows boot.");}
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}File()=default;File(const File&)=delete;};
struct DeviceSet {HDEVINFO value=INVALID_HANDLE_VALUE;~DeviceSet(){if(value!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(value);}};
struct Key {HKEY value=nullptr;~Key(){if(value)RegCloseKey(value);}};
struct Packet {Wire::Header h;Bytes data;};
std::shared_ptr<TitanTransport::Channel> OwnChannel(SOCKET socket){
 auto raw=new(std::nothrow)TitanTransport::Channel(socket);
 if(!raw){closesocket(socket);throw std::bad_alloc();}
 return std::shared_ptr<TitanTransport::Channel>(raw);
}
struct Shared {
 TitanTransport::Runtime network;
 mutable std::mutex stateMutex,writeMutex;
 Status status;
 std::atomic_bool running{false};
 std::shared_ptr<TitanTransport::Channel> channel;
 void Detail(std::string text){std::lock_guard<std::mutex> lock(stateMutex);status.detail=std::move(text);}
 void Count(bool success){std::lock_guard<std::mutex> lock(stateMutex);++status.requests;if(success)++status.completed;else ++status.failed;}
 Status Snapshot() const {std::lock_guard<std::mutex> lock(stateMutex);auto s=status;s.active=running.load();return s;}
 void Connected(bool value){std::lock_guard<std::mutex> lock(stateMutex);status.connected=value;}
 void SetChannel(std::shared_ptr<TitanTransport::Channel> c){std::lock_guard<std::mutex> lock(stateMutex);channel=std::move(c);}
 std::shared_ptr<TitanTransport::Channel> GetChannel(){std::lock_guard<std::mutex> lock(stateMutex);return channel;}
 void CloseChannel() noexcept {try{auto c=GetChannel();if(c)c->Close();}catch(...){}}
 void Send(const Wire::Header& h,const Bytes& data={}){
  if(!Wire::Valid(h)||data.size()!=h.size)throw std::runtime_error("Refused an invalid outbound USB bridge frame.");
  std::lock_guard<std::mutex> lock(writeMutex);auto c=GetChannel();if(!c)throw std::runtime_error("The encrypted USB connection is closed.");
  Bytes frame(sizeof(h)+data.size());std::memcpy(frame.data(),&h,sizeof(h));if(!data.empty())std::memcpy(frame.data()+sizeof(h),data.data(),data.size());c->Send(frame.data(),frame.size());
 }
 Packet Receive(uint32_t expectedType=0){Packet p;auto c=GetChannel();if(!c)throw std::runtime_error("The encrypted USB connection is closed.");auto start=Clock::now();c->Receive(&p.h,sizeof(p.h),5000);if(!Wire::Valid(p.h)||(expectedType&&p.h.type!=expectedType))throw std::runtime_error("The peer sent an invalid, unexpected or oversized USB bridge frame.");p.data.resize(p.h.size);if(!p.data.empty()){auto spent=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count();if(spent>=5000)throw std::runtime_error("The complete USB frame exceeded its five-second deadline.");c->Receive(p.data.data(),p.data.size(),uint32_t(5000-spent));}return p;}
};
std::wstring RegText(HDEVINFO set,SP_DEVINFO_DATA& dev,DWORD property){
 wchar_t text[1024]{};DWORD type=0,bytes=sizeof(text);if(!SetupDiGetDeviceRegistryPropertyW(set,&dev,property,&type,reinterpret_cast<BYTE*>(text),bytes,nullptr)||type!=REG_SZ)return {};return text;
}
bool OwnVirtualAncestry(DEVINST node){for(unsigned n=0;n<32;++n){wchar_t service[256]{};ULONG type=0,bytes=sizeof(service);if(CM_Get_DevNode_Registry_PropertyW(node,CM_DRP_SERVICE,&type,service,&bytes,0)==CR_SUCCESS&&type==REG_SZ&&!_wcsicmp(service,L"TitanUSBOverIP"))return true;DEVINST parent=0;if(CM_Get_Parent(&parent,node,0)!=CR_SUCCESS)return false;node=parent;}return true;}

Bytes Descriptor(WINUSB_INTERFACE_HANDLE usb,UCHAR type,UCHAR index,USHORT language,ULONG limit){
 Bytes bytes(limit);ULONG count=0;if(!WinUsb_GetDescriptor(usb,type,index,language,bytes.data(),limit,&count))Fail("Read WinUSB descriptor");if(count>limit)throw std::runtime_error("WinUSB returned a descriptor beyond its requested bound.");bytes.resize(count);return bytes;
}
std::vector<Wire::Descriptor> ReadDescription(WINUSB_INTERFACE_HANDLE usb,uint32_t& speed,std::vector<UCHAR>& endpoints){
 ULONG bytes=1;UCHAR physicalSpeed=0;if(!WinUsb_QueryDeviceInformation(usb,DEVICE_SPEED,&bytes,&physicalSpeed)||bytes!=1)Fail("Read WinUSB device speed");
 if(physicalSpeed!=FullSpeed&&physicalSpeed!=HighSpeed)throw std::runtime_error("This edition supports full-speed or high-speed USB 2 devices only.");speed=physicalSpeed==HighSpeed?VdHighSpeed:VdFullSpeed;
 auto device=Descriptor(usb,1,0,0,18);auto config=Descriptor(usb,2,0,0,9);
 if(device.size()!=18||config.size()!=9)throw std::runtime_error("The USB device/configuration header is incomplete.");
 const ULONG total=config[2]|(ULONG(config[3])<<8);if(total<9||total>16384)throw std::runtime_error("The USB configuration exceeds the supported 16 KiB bound.");config=Descriptor(usb,2,0,0,total);
 if(config.size()!=total||device[17]!=1||config[4]!=1)throw std::runtime_error("Only one configuration with one vendor-specific interface is supported.");
 USB_INTERFACE_DESCRIPTOR iface{};if(!WinUsb_QueryInterfaceSettings(usb,0,&iface))Fail("Read WinUSB interface");
 if(iface.bInterfaceNumber||iface.bAlternateSetting||iface.bInterfaceClass!=255||!iface.bNumEndpoints||iface.bNumEndpoints>8)throw std::runtime_error("Select a vendor-specific WinUSB interface 0, alternate setting 0, with 1..8 bulk/interrupt endpoints.");
 for(UCHAR i=0;i<iface.bNumEndpoints;++i){WINUSB_PIPE_INFORMATION pipe{};if(!WinUsb_QueryPipe(usb,0,i,&pipe))Fail("Read WinUSB pipe");if(pipe.PipeType!=UsbdPipeTypeBulk&&pipe.PipeType!=UsbdPipeTypeInterrupt)throw std::runtime_error("Isochronous and unsupported endpoint types are refused.");endpoints.push_back(pipe.PipeId);ULONG timeout=3000;if(!WinUsb_SetPipePolicy(usb,pipe.PipeId,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout))Fail("Set bounded WinUSB transfer timeout");}
 ULONG timeout=3000;if(!WinUsb_SetPipePolicy(usb,0,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout))Fail("Set bounded WinUSB control timeout");
 std::vector<Wire::Descriptor> result;result.reserve(8);result.push_back({0,0,std::move(device)});result.push_back({0,0,std::move(config)});
 auto& d=result[0].data;
 if(d[14]||d[15]||d[16]){
  auto languages=Descriptor(usb,3,0,0,254);if(languages.size()<4||languages[1]!=3)throw std::runtime_error("The USB string-language descriptor is incomplete.");
  uint16_t language=uint16_t(languages[2]|(uint16_t(languages[3])<<8));result.push_back({0,0,std::move(languages)});
  std::vector<UCHAR> seen;for(unsigned i=14;i<17;++i)if(d[i]&&std::find(seen.begin(),seen.end(),d[i])==seen.end()){auto string=Descriptor(usb,3,d[i],language,254);if(string.size()<2||string[1]!=3)throw std::runtime_error("A USB identity string is incomplete.");result.push_back({d[i],language,std::move(string)});seen.push_back(d[i]);}
 }
 // Replace the optional OS discovery string for the virtual child only. This
 // provides an inbox WinUSB interface without installing a physical driver.
 result.push_back({0xee,0,{18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,0x5b,0}});
 return result;
}
std::string DescriptionFailure(const Bytes& blob,int code,uint32_t line){
 std::ostringstream out;out<<"USB description rejected (validation code "<<code<<", check line "<<line<<"). ";
 if(blob.size()<sizeof(VD_CREATE))return out.str()+"The create header is truncated.";
 const auto* create=reinterpret_cast<const VD_CREATE*>(blob.data());out<<"Bus speed="<<create->Speed<<" (2=full,3=high), flags="<<create->Flags<<", descriptor count="<<create->DescriptorCount<<", rule count="<<create->RuleCount<<". ";
 if(create->DescriptorsOffset>blob.size()||create->DescriptorCount>(blob.size()-create->DescriptorsOffset)/sizeof(VD_DESCRIPTOR))return out.str()+"The descriptor table exceeds the supplied buffer.";
 const auto* table=reinterpret_cast<const VD_DESCRIPTOR*>(blob.data()+create->DescriptorsOffset);
 for(uint32_t i=0;i<create->DescriptorCount;++i){const auto& d=table[i];out<<"Descriptor["<<i<<"] index="<<d.Index<<", language="<<d.Language<<", length="<<d.Length;
  if(d.Offset>blob.size()||d.Length>blob.size()-d.Offset){out<<" exceeds the buffer. ";continue;}const auto* bytes=blob.data()+d.Offset;
  if(d.Length>=2)out<<", type="<<unsigned(bytes[1]);
  if(d.Length==18&&bytes[1]==1)out<<", bcdUSB="<<(bytes[2]|(unsigned(bytes[3])<<8))<<", deviceClass="<<unsigned(bytes[4])<<", endpoint0Packet="<<unsigned(bytes[7])<<", configurations="<<unsigned(bytes[17]);
  if(d.Length>=9&&bytes[1]==2){out<<", wTotalLength="<<(bytes[2]|(unsigned(bytes[3])<<8))<<", interfaces="<<unsigned(bytes[4])<<", configuration="<<unsigned(bytes[5])<<", attributes="<<unsigned(bytes[7])<<", maxPower="<<unsigned(bytes[8]);
   for(size_t offset=9;offset+2<=d.Length;){const auto n=bytes[offset];if(n<2||n>d.Length-offset){out<<", invalid nested length="<<unsigned(n)<<" at "<<offset;break;}
    if(bytes[offset+1]==4&&n>=9)out<<"; interface="<<unsigned(bytes[offset+2])<<", alternate="<<unsigned(bytes[offset+3])<<", endpoints="<<unsigned(bytes[offset+4])<<", class="<<unsigned(bytes[offset+5]);
    if(bytes[offset+1]==5&&n>=7)out<<"; endpoint="<<unsigned(bytes[offset+2])<<", transferType="<<unsigned(bytes[offset+3]&3)<<", packet="<<(bytes[offset+4]|(unsigned(bytes[offset+5])<<8))<<", interval="<<unsigned(bytes[offset+6]);offset+=n;
   }
  }out<<". ";
 }return out.str();
}
DWORD Ioctl(HANDLE file,DWORD code,const void* in,DWORD size,void* out,DWORD capacity,DWORD& bytes,DWORD timeout=5000){
 File event;event.value=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event.value)return GetLastError();OVERLAPPED op{};op.hEvent=event.value;bytes=0;
 if(DeviceIoControl(file,code,const_cast<void*>(in),size,out,capacity,&bytes,&op))return 0;
 DWORD error=GetLastError();if(error!=ERROR_IO_PENDING)return error;
 DWORD waited=WaitForSingleObject(event.value,timeout);
 if(waited!=WAIT_OBJECT_0){DWORD cause=waited==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();CancelIoEx(file,&op);if(GetOverlappedResult(file,&op,&bytes,TRUE))return 0;error=GetLastError();return error==ERROR_OPERATION_ABORTED?cause:error;}
 return GetOverlappedResult(file,&op,&bytes,FALSE)?0:GetLastError();
}
ULONG DriverError(DWORD error){return error==ERROR_SUCCESS?0:error==ERROR_SEM_TIMEOUT||error==ERROR_TIMEOUT?2:error==ERROR_DEVICE_NOT_CONNECTED||error==ERROR_BAD_DEVICE?3:error==ERROR_OPERATION_ABORTED?4:error==ERROR_GEN_FAILURE?1:5;}
}

std::vector<Device> Devices(){
 DeviceSet set;set.value=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_ALLCLASSES|DIGCF_PRESENT);if(set.value==INVALID_HANDLE_VALUE)Fail("Enumerate Windows devices");std::vector<Device> result;
 for(DWORD i=0;;++i){SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);if(!SetupDiEnumDeviceInfo(set.value,i,&dev)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)Fail("Read Windows device inventory");break;}
  auto service=RegText(set.value,dev,SPDRP_SERVICE);if(_wcsicmp(service.c_str(),L"WinUSB"))continue;
  if(OwnVirtualAncestry(dev.DevInst))continue;
  wchar_t instance[4096]{};if(!SetupDiGetDeviceInstanceIdW(set.value,&dev,instance,4096,nullptr))continue;
  Key key;key.value=SetupDiOpenDevRegKey(set.value,&dev,DICS_FLAG_GLOBAL,0,DIREG_DEV,KEY_READ);if(key.value==INVALID_HANDLE_VALUE){key.value=nullptr;continue;}
  Key parameters;(void)RegOpenKeyExW(key.value,L"Device Parameters",0,KEY_READ,&parameters.value);
  wchar_t guids[4096]{};DWORD type=0,size=sizeof(guids);HKEY source=parameters.value?parameters.value:key.value;LONG registryStatus=RegQueryValueExW(source,L"DeviceInterfaceGUIDs",nullptr,&type,reinterpret_cast<BYTE*>(guids),&size);if(registryStatus!=ERROR_SUCCESS){size=sizeof(guids);registryStatus=RegQueryValueExW(source,L"DeviceInterfaceGUID",nullptr,&type,reinterpret_cast<BYTE*>(guids),&size);}if(registryStatus!=ERROR_SUCCESS||(type!=REG_MULTI_SZ&&type!=REG_SZ)||size>sizeof(guids)-2||size%2)continue;
  const size_t chars=size/2;for(size_t pos=0;pos<chars&&guids[pos];){size_t end=pos;while(end<chars&&guids[end])++end;if(end==chars)break;GUID guid{};if(SUCCEEDED(CLSIDFromString(guids+pos,&guid))){ULONG count=0;if(CM_Get_Device_Interface_List_SizeW(&count,&guid,instance,CM_GET_DEVICE_INTERFACE_LIST_PRESENT)==CR_SUCCESS&&count>1&&count<65536){std::vector<wchar_t> paths(count+2);if(CM_Get_Device_Interface_ListW(&guid,instance,paths.data(),count,CM_GET_DEVICE_INTERFACE_LIST_PRESENT)==CR_SUCCESS){for(const wchar_t* path=paths.data();*path;path+=wcslen(path)+1){auto name=RegText(set.value,dev,SPDRP_FRIENDLYNAME);if(name.empty())name=RegText(set.value,dev,SPDRP_DEVICEDESC);result.push_back({name,path});}}}}pos=end+1;}
 }
 return result;
}
std::string Verification(std::string_view key){return TitanSdk::Verification(TITAN_SDK_USB_OVER_IP,key);}

struct Exporter::State:Shared {
 File file;WINUSB_INTERFACE_HANDLE usb=nullptr;
 TitanTransport::Identity identity;
 Pairing pairing;
 Bytes description;
 std::atomic<SOCKET> listener{INVALID_SOCKET};
 std::thread worker,heartbeat;
 struct Job {Packet packet;OVERLAPPED op{};std::atomic_bool canceled{false};std::mutex ioMutex;bool active=false;void Cancel(HANDLE file){canceled=true;std::lock_guard<std::mutex> lock(ioMutex);if(active)CancelIoEx(file,&op);}};
 struct Pipe {UCHAR address=0;std::mutex mutex;std::condition_variable ready;std::deque<std::shared_ptr<Job>> queue;std::thread worker;};
 std::vector<std::unique_ptr<Pipe>> pipes;
 std::mutex jobsMutex;std::map<uint64_t,std::shared_ptr<Job>> jobs;
 ~State(){if(usb)WinUsb_Free(usb);}
 void CancelJobs() noexcept {CancelIoEx(file.value,nullptr);for(auto& p:pipes)p->ready.notify_all();}
 void JoinPipes(){for(auto& p:pipes)if(p->worker.joinable())p->worker.join();}
 Packet Execute(const std::shared_ptr<Job>& job){
  auto request=job->packet;Packet response;response.h=request.h;response.h.type=Wire::Response;response.h.capacity=0;response.h.size=0;
  if(job->canceled){response.h.error=ERROR_OPERATION_ABORTED;return response;}
  bool in=request.h.flags?bool(request.h.setup[0]&128):bool(request.h.endpoint&128);Bytes data=in?Bytes(request.h.capacity):request.data;
  File event;event.value=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event.value){response.h.error=GetLastError();return response;}job->op.hEvent=event.value;ULONG done=0;BOOL success;
  DWORD error=0;
  {std::lock_guard<std::mutex> lock(job->ioMutex);if(job->canceled){response.h.error=ERROR_OPERATION_ABORTED;return response;}
   if(request.h.flags){WINUSB_SETUP_PACKET setup{};std::memcpy(&setup,request.h.setup,8);success=WinUsb_ControlTransfer(usb,setup,data.empty()?nullptr:data.data(),request.h.capacity,&done,&job->op);}
   else if(in)success=WinUsb_ReadPipe(usb,UCHAR(request.h.endpoint),data.empty()?nullptr:data.data(),request.h.capacity,&done,&job->op);
   else success=WinUsb_WritePipe(usb,UCHAR(request.h.endpoint),data.empty()?nullptr:data.data(),request.h.capacity,&done,&job->op);
   error=success?0:GetLastError();job->active=!success&&error==ERROR_IO_PENDING;
  }
  if(!success&&error==ERROR_IO_PENDING){DWORD waited=WaitForSingleObject(event.value,3500);if(waited!=WAIT_OBJECT_0)CancelIoEx(file.value,&job->op);success=WinUsb_GetOverlappedResult(usb,&job->op,&done,TRUE);error=success?0:GetLastError();if(waited==WAIT_TIMEOUT&&!success&&error==ERROR_OPERATION_ABORTED)error=ERROR_TIMEOUT;}
  {std::lock_guard<std::mutex> lock(job->ioMutex);job->active=false;}
  if(done>request.h.capacity){success=FALSE;error=ERROR_INVALID_DATA;done=0;}
  if(success){response.h.capacity=done;if(in){data.resize(done);response.data=std::move(data);response.h.size=done;}}
  else response.h.error=error?error:ERROR_GEN_FAILURE;
  return response;
 }
 void PipeWorker(Pipe& pipe){
  for(;;){std::shared_ptr<Job> job;{std::unique_lock<std::mutex> lock(pipe.mutex);pipe.ready.wait(lock,[&]{return !running||!pipe.queue.empty();});if(!running)break;job=std::move(pipe.queue.front());pipe.queue.pop_front();}
   try{auto response=Execute(job);{std::lock_guard<std::mutex> lock(jobsMutex);jobs.erase(response.h.id);}Count(response.h.error==0);if(running)Send(response.h,response.data);}
   catch(const std::exception& e){if(running)Detail(e.what());running=false;CloseChannel();CancelJobs();break;}
  }
 }
 void Serve(){
  try{
   bool authenticated=false;auto licenseCheck=Clock::now();
   while(running&&!authenticated){
    SOCKET accepted=INVALID_SOCKET;
    while(running&&accepted==INVALID_SOCKET){
     if(Clock::now()-licenseCheck>=std::chrono::seconds(1)){RequireLicense(false);licenseCheck=Clock::now();}
     SOCKET listen=listener.load();if(listen==INVALID_SOCKET)break;fd_set read;FD_ZERO(&read);FD_SET(listen,&read);timeval wait{0,200000};int ready=select(0,&read,nullptr,nullptr,&wait);if(ready==SOCKET_ERROR){if(!running)return;Fail("Wait for USB peer",DWORD(WSAGetLastError()));}if(ready)accepted=accept(listen,nullptr,nullptr);
    }
    if(accepted==INVALID_SOCKET)return;
    try{
     auto acceptedChannel=OwnChannel(accepted);SetChannel(acceptedChannel);acceptedChannel->HandshakeServer(identity);
     if(!running)break;
     auto hello=Receive(Wire::Hello);if(hello.h.type!=Wire::Hello||!TitanTransport::ConstantTimeEqual(std::string_view(reinterpret_cast<const char*>(hello.data.data()),hello.data.size()),pairing.secret))throw std::runtime_error("The peer did not supply the current session secret.");
     RequireLicense(false);authenticated=true;
    }catch(const std::exception& e){
     CloseChannel();SetChannel({});if(!running)break;
     Detail(std::string("Unauthenticated peer refused; waiting for the intended recipient. ")+e.what());
     for(unsigned pause=0;pause<5&&running;++pause)std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
   }
   if(!running||!authenticated)return;
   Wire::Header h;h.type=Wire::Description;h.size=uint32_t(description.size());Send(h,description);Connected(true);Detail("Encrypted USB peer connected. Only the selected WinUSB interface is exported.");
   for(auto& pipe:pipes)pipe->worker=std::thread([this,p=pipe.get()]{PipeWorker(*p);});
   heartbeat=std::thread([this]{try{while(running){for(unsigned i=0;i<10&&running;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));if(!running)break;RequireLicense(false);Wire::Header h;h.type=Wire::Ping;Send(h);}}catch(const std::exception& e){if(running)Detail(e.what());running=false;CloseChannel();CancelJobs();}});
   while(running){auto p=Receive();if(p.h.type==Wire::Ping)continue;if(p.h.type==Wire::Goodbye)break;
    if(p.h.type==Wire::Cancel){std::lock_guard<std::mutex> lock(jobsMutex);auto found=jobs.find(p.h.id);if(found!=jobs.end())found->second->Cancel(file.value);continue;}
    if(p.h.type!=Wire::Request)throw std::runtime_error("Unexpected frame after USB peer authentication.");
    auto pipe=std::find_if(pipes.begin(),pipes.end(),[&](const auto& x){return x->address==p.h.endpoint;});if(pipe==pipes.end())throw std::runtime_error("The USB peer requested an endpoint absent from the exported interface.");
    auto job=std::make_shared<Job>();job->packet=std::move(p);
    {std::lock_guard<std::mutex> lock(jobsMutex);if(jobs.size()>=32||jobs.count(job->packet.h.id))throw std::runtime_error("The USB peer exceeded its 32-request bound or repeated an active request ID.");jobs.emplace(job->packet.h.id,job);}
    {std::lock_guard<std::mutex> lock((*pipe)->mutex);(*pipe)->queue.push_back(job);}(*pipe)->ready.notify_one();
   }
  }catch(const std::exception& e){if(running)Detail(e.what());}
  running=false;Connected(false);CloseChannel();CancelJobs();JoinPipes();if(heartbeat.joinable())heartbeat.join();
 }
};
Exporter::Exporter():state_(std::make_unique<State>()){}
Exporter::~Exporter(){Stop();}
Pairing Exporter::Start(const std::wstring& path,std::string_view bind,uint16_t port){
 std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);
 Stop();state_=std::make_unique<State>();auto& s=*state_;
 struct Guard{Exporter* owner;bool committed=false;~Guard(){if(!committed)owner->Stop();}} guard{this};
 if(path.size()<4||path.size()>4096||path.compare(0,4,L"\\\\?\\")!=0)throw std::runtime_error("Select one exact existing WinUSB device-interface path from Devices().");
 auto devices=Devices();if(std::none_of(devices.begin(),devices.end(),[&](const Device& d){return !_wcsicmp(d.path.c_str(),path.c_str());}))throw std::runtime_error("The selected interface is not a present supported WinUSB device. Re-exporting this SDK's own virtual child is refused.");
 s.file.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(s.file.value==INVALID_HANDLE_VALUE)Fail("Open the selected WinUSB interface exclusively");if(!WinUsb_Initialize(s.file.value,&s.usb))Fail("Initialize the selected WinUSB interface (its existing function driver must be WinUSB)");
 uint32_t speed=0;std::vector<UCHAR> endpoints;auto descriptors=ReadDescription(s.usb,speed,endpoints);s.description=Wire::DescriptionBlob(descriptors,speed);VD_LAYOUT layout{};uint32_t invalidLine=0;const int validation=VdValidateCreate(s.description.data(),uint32_t(s.description.size()),&layout,&invalidLine);if(validation)throw std::runtime_error(DescriptionFailure(s.description,validation,invalidLine));
 auto control=std::make_unique<State::Pipe>();control->address=0;s.pipes.push_back(std::move(control));for(auto endpoint:endpoints){auto pipe=std::make_unique<State::Pipe>();pipe->address=endpoint;s.pipes.push_back(std::move(pipe));}
 RequireLicense(true);s.pairing.address=std::string(bind);s.pairing.fingerprint=s.identity.Fingerprint();s.pairing.secret=TitanTransport::RandomSecret();s.listener=TitanTransport::Listen(bind,port,&s.pairing.port);s.running=true;s.Detail("Waiting for one pinned, authenticated peer. Stop closes this exported interface.");
 try{s.worker=std::thread([&s]{s.Serve();});}catch(...){s.running=false;SOCKET value=s.listener.exchange(INVALID_SOCKET);if(value!=INVALID_SOCKET)closesocket(value);throw;}guard.committed=true;return s.pairing;
}
Status Exporter::GetStatus()const{std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);return state_->Snapshot();}
void Exporter::Stop() noexcept{std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);if(!state_)return;auto& s=*state_;s.running=false;SOCKET socket=s.listener.exchange(INVALID_SOCKET);if(socket!=INVALID_SOCKET)closesocket(socket);s.CloseChannel();s.CancelJobs();if(s.worker.joinable())s.worker.join();s.Connected(false);if(s.usb){WinUsb_Free(s.usb);s.usb=nullptr;}if(s.file.value!=INVALID_HANDLE_VALUE){CloseHandle(s.file.value);s.file.value=INVALID_HANDLE_VALUE;}}

struct Client::State:Shared {
 File driver;std::atomic<uint64_t> device{0};
 std::thread pump,receiver;
 std::mutex pendingMutex;
 std::map<uint64_t,Wire::Header> pending;
 std::deque<uint64_t> canceled;
 void Complete(const Packet& p,const Wire::Header& original){
  const bool in=original.flags?bool(original.setup[0]&128):bool(original.endpoint&128);
  if(p.h.endpoint!=original.endpoint||p.h.flags!=original.flags||p.h.capacity>original.capacity||(in?p.h.size!=p.h.capacity:p.h.size!=0))throw std::runtime_error("A remote USB completion does not match its original request.");
  VD_RESPONSE h{};h.Id=device;h.RequestId=p.h.id;h.Stall=DriverError(p.h.error);h.Length=p.h.size;h.Completed=p.h.capacity;
  Bytes bytes(sizeof(h)+p.data.size());std::memcpy(bytes.data(),&h,sizeof(h));if(!p.data.empty())std::memcpy(bytes.data()+sizeof(h),p.data.data(),p.data.size());DWORD done=0,error=Ioctl(driver.value,VD_RESPOND,bytes.data(),DWORD(bytes.size()),nullptr,0,done);if(error&&error!=ERROR_NOT_FOUND&&running)Fail("Complete the owned virtual USB request",error);Count(!p.h.error&&!error);
 }
 void ReceiveWorker(){try{while(running){auto p=Receive();if(p.h.type==Wire::Ping)continue;if(p.h.type!=Wire::Response)throw std::runtime_error("The USB exporter sent an unexpected post-connection frame.");Wire::Header original;bool found=false;
    {std::lock_guard<std::mutex> lock(pendingMutex);auto it=pending.find(p.h.id);if(it!=pending.end()){original=it->second;pending.erase(it);found=true;}else if(std::find(canceled.begin(),canceled.end(),p.h.id)==canceled.end())throw std::runtime_error("The exporter completed an unknown USB request ID.");}
    if(found)Complete(p,original);
   }}catch(const std::exception& e){if(running)Detail(e.what());running=false;CloseChannel();CancelIoEx(driver.value,nullptr);}}
 void RemoveDevice()noexcept{uint64_t id=device.exchange(0);if(id&&driver.value!=INVALID_HANDLE_VALUE){VD_ID value{id};DWORD done=0;(void)Ioctl(driver.value,VD_DELETE_DEVICE,&value,sizeof(value),nullptr,0,done,6000);}}
 void Pump(){auto ping=Clock::now();try{while(running){VD_OUTPUT output{};DWORD done=0,error=Ioctl(driver.value,VD_RECEIVE,nullptr,0,&output,sizeof(output),done,200);
    if(error&&error!=ERROR_TIMEOUT){if(running)Fail("Receive an owned virtual USB request",error);break;}
    if(!error){if(done<offsetof(VD_OUTPUT,Data)||output.Length>VD_MAX_TRANSFER||done!=offsetof(VD_OUTPUT,Data)+output.Length||output.Id!=device)throw std::runtime_error("The USB driver returned an inconsistent request record.");
     if(output.Kind==VdTransferCancel){bool known=false;{std::lock_guard<std::mutex> lock(pendingMutex);known=pending.erase(output.RequestId)>0;canceled.push_back(output.RequestId);if(canceled.size()>256)canceled.pop_front();}if(known){Wire::Header h;h.type=Wire::Cancel;h.id=output.RequestId;h.endpoint=output.Endpoint;Send(h);}continue;}
     if(output.Kind==VdDeviceState)continue;
     if(output.Kind!=VdControlRequest&&output.Kind!=VdTransferRequest)throw std::runtime_error("Unexpected virtual USB request kind.");
     Wire::Header h;h.type=Wire::Request;h.id=output.RequestId;h.endpoint=output.Endpoint;h.flags=output.Kind==VdControlRequest?1u:0u;h.size=output.Length;h.capacity=output.Reserved;std::memcpy(h.setup,output.Setup,8);
     {std::lock_guard<std::mutex> lock(pendingMutex);if(pending.size()>=32||!pending.emplace(h.id,h).second)throw std::runtime_error("The virtual USB pending-request bound was exceeded.");}
     Send(h,Wire::Bytes(output.Data,output.Length));
    }
    if(Clock::now()-ping>=std::chrono::seconds(1)){RequireLicense(false);Wire::Header h;h.type=Wire::Ping;Send(h);ping=Clock::now();}
   }}catch(const std::exception& e){if(running)Detail(e.what());running=false;CloseChannel();CancelIoEx(driver.value,nullptr);}RemoveDevice();Connected(false);
 }
};
Client::Client():state_(std::make_unique<State>()){}
Client::~Client(){Stop();}
void Client::Connect(const Pairing& p){
 std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);
 Stop();state_=std::make_unique<State>();auto& s=*state_;RequireLicense(true);if(!TitanTransport::IsFingerprint(p.fingerprint)||!TitanTransport::IsSecret(p.secret)||!p.port)throw std::runtime_error("Enter the current exporter IPv4 address, port, SHA-256 fingerprint and 32-character session secret.");
 struct Guard{Client* owner;bool committed=false;~Guard(){if(!committed)owner->Stop();}} guard{this};
 s.driver.value=CreateFileW(VD_PATH,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(s.driver.value==INVALID_HANDLE_VALUE)Fail("Open TitanUSBOverIP (install both signed packages and run elevated)");
 auto channel=OwnChannel(TitanTransport::Connect(p.address,p.port));s.SetChannel(channel);channel->HandshakeClient(p.fingerprint);
 Wire::Header hello;hello.type=Wire::Hello;hello.size=uint32_t(p.secret.size());s.Send(hello,Wire::Bytes(p.secret.data(),p.secret.size()));auto packet=s.Receive(Wire::Description);if(packet.h.type!=Wire::Description)throw std::runtime_error("The exporter did not return an authenticated USB device description.");
 VD_LAYOUT layout{};uint32_t invalidLine=0;const int validation=VdValidateCreate(packet.data.data(),uint32_t(packet.data.size()),&layout,&invalidLine);if(validation)throw std::runtime_error(DescriptionFailure(packet.data,validation,invalidLine));if(layout.EndpointCount>8)throw std::runtime_error("The USB description has "+std::to_string(layout.EndpointCount)+" endpoints; this edition permits at most8.");VD_ID created{};DWORD done=0;DWORD error=Ioctl(s.driver.value,VD_CREATE_DEVICE,packet.data.data(),DWORD(packet.data.size()),&created,sizeof(created),done,10000);if(error)Fail("Create the authenticated owned virtual USB device",error);if(done!=sizeof(created)||!created.Id)throw std::runtime_error("The driver returned an invalid virtual USB identity.");s.device=created.Id;s.running=true;s.Connected(true);s.Detail("Encrypted USB device connected. Windows uses the virtual child's inbox WinUSB interface.");
 try{s.receiver=std::thread([&s]{s.ReceiveWorker();});s.pump=std::thread([&s]{s.Pump();});}catch(...){Stop();throw;}guard.committed=true;
}
Status Client::GetStatus()const{std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);return state_->Snapshot();}
void Client::Stop()noexcept{std::lock_guard<std::recursive_mutex> lifecycle(lifecycle_);if(!state_)return;auto& s=*state_;s.running=false;s.CloseChannel();if(s.driver.value!=INVALID_HANDLE_VALUE)CancelIoEx(s.driver.value,nullptr);if(s.pump.joinable())s.pump.join();if(s.receiver.joinable())s.receiver.join();s.RemoveDevice();if(s.driver.value!=INVALID_HANDLE_VALUE){CloseHandle(s.driver.value);s.driver.value=INVALID_HANDLE_VALUE;}s.Connected(false);}
}
