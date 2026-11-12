#pragma once
#include "USBProtocol.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace UsbIp::Wire {
constexpr uint32_t Magic=0x31504955u,Version=1,MaxPayload=65536;
enum :uint32_t { Hello=1,Description=2,Request=3,Response=4,Goodbye=5,Ping=6,Cancel=7 };
#pragma pack(push,1)
struct Header {
 uint32_t magic=Magic,version=Version,type=0,size=0;
 uint64_t id=0;
 uint32_t endpoint=0,capacity=0,error=0,flags=0;
 uint8_t setup[8]{};
};
#pragma pack(pop)
static_assert(sizeof(Header)==48);
inline bool Valid(const Header& h) noexcept {
 if(h.magic!=Magic||h.version!=Version||h.type<Hello||h.type>Cancel||h.size>MaxPayload||h.endpoint>255||h.flags>1)return false;
 if(h.type==Hello)return h.size==32&&!h.id&&!h.endpoint&&!h.capacity&&!h.error&&!h.flags;
 if(h.type==Description)return h.size>=sizeof(VD_CREATE)&&!h.id&&!h.endpoint&&!h.capacity&&!h.error&&!h.flags;
 if(h.type==Goodbye||h.type==Ping)return !h.size&&!h.id&&!h.endpoint&&!h.capacity&&!h.error&&!h.flags;
 if(h.type==Cancel)return h.id&&!h.size&&!h.capacity&&!h.error;
 if(!h.id||h.capacity>VD_MAX_TRANSFER||h.size>VD_MAX_TRANSFER)return false;
 if(h.type==Request){
  if(h.error||(h.endpoint==0)!=bool(h.flags))return false;
  const bool in=h.flags?bool(h.setup[0]&128):bool(h.endpoint&128);
  if(h.flags){uint32_t length=h.setup[6]|(uint32_t(h.setup[7])<<8);if(h.capacity!=length||((h.setup[0]&0x60)!=0x20&&(h.setup[0]&0x60)!=0x40))return false;}
  else if((h.endpoint&0x70)||!(h.endpoint&15))return false;
  return in?h.size==0:h.size==h.capacity;
 }
 return h.error?h.size==0&&h.capacity==0:h.size<=h.capacity;
}
inline std::vector<uint8_t> Bytes(const void* p,size_t count){
 auto b=static_cast<const uint8_t*>(p);return count?std::vector<uint8_t>(b,b+count):std::vector<uint8_t>{};
}
inline void Append16(std::vector<uint8_t>& b,uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
inline void Append32(std::vector<uint8_t>& b,uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(v>>(8*i)));}
inline void AppendText(std::vector<uint8_t>& b,const wchar_t* s,size_t chars){for(size_t i=0;i<chars;++i)Append16(b,uint16_t(s[i]));}
// Microsoft OS 1.0 descriptors bind only the virtual child to inbox WinUSB.
// Physical-device drivers are never installed or replaced by this SDK.
inline std::vector<uint8_t> CompatibleId(){
 std::vector<uint8_t> b;Append32(b,40);Append16(b,0x100);Append16(b,4);b.push_back(1);b.resize(16);
 b.push_back(0);b.push_back(1);const char id[8]={'W','I','N','U','S','B',0,0};b.insert(b.end(),id,id+8);b.resize(40);return b;
}
inline constexpr wchar_t InterfaceGuid[]=L"{ED329644-7E18-44D6-83C2-052C217393A7}";
inline std::vector<uint8_t> ExtendedProperties(){
 constexpr wchar_t name[]=L"DeviceInterfaceGUID";constexpr size_t nameChars=sizeof(name)/sizeof(wchar_t);
 constexpr size_t guidChars=sizeof(InterfaceGuid)/sizeof(wchar_t);
 const uint32_t propertySize=4+4+2+uint32_t(nameChars*2)+4+uint32_t(guidChars*2);
 std::vector<uint8_t> b;Append32(b,10+propertySize);Append16(b,0x100);Append16(b,5);Append16(b,1);
 Append32(b,propertySize);Append32(b,1);Append16(b,uint16_t(nameChars*2));AppendText(b,name,nameChars);
 Append32(b,uint32_t(guidChars*2));AppendText(b,InterfaceGuid,guidChars);return b;
}
struct Descriptor {uint32_t index=0,language=0;std::vector<uint8_t> data;};
inline std::vector<uint8_t> DescriptionBlob(const std::vector<Descriptor>& descriptors,uint32_t speed){
 auto compatible=CompatibleId(),properties=ExtendedProperties();
 const size_t head=sizeof(VD_CREATE)+descriptors.size()*sizeof(VD_DESCRIPTOR)+2*sizeof(VD_CONTROL_RULE);
 size_t total=head+compatible.size()+properties.size();for(const auto& d:descriptors)total+=d.data.size();
 if(descriptors.size()>VD_MAX_DESCRIPTORS||total>VD_MAX_CREATE)throw std::runtime_error("USB descriptor set exceeds the bounded device description.");
 std::vector<uint8_t> b(total);VD_CREATE h{};h.Version=VD_VERSION;h.TotalSize=uint32_t(total);h.Speed=speed;h.Flags=VdDelegateControls|VdDelegateTransfers;
 h.DescriptorCount=uint32_t(descriptors.size());h.DescriptorsOffset=sizeof(h);h.RuleCount=2;h.RulesOffset=uint32_t(sizeof(h)+descriptors.size()*sizeof(VD_DESCRIPTOR));h.InputDepth=1;std::memcpy(b.data(),&h,sizeof(h));
 size_t cursor=head;
 for(size_t i=0;i<descriptors.size();++i){const auto& d=descriptors[i];VD_DESCRIPTOR v{VdStandardDescriptor,d.index,d.language,0,uint32_t(cursor),uint32_t(d.data.size())};std::memcpy(b.data()+h.DescriptorsOffset+i*sizeof(v),&v,sizeof(v));std::memcpy(b.data()+cursor,d.data.data(),d.data.size());cursor+=d.data.size();}
 // Compatible ID is device-recipient; Extended Properties is interface-recipient.
 for(unsigned i=0;i<2;++i){const auto& payload=i?properties:compatible;VD_CONTROL_RULE r{};r.Setup[0]=uint8_t(i?0xc1:0xc0);r.Setup[1]=0x5b;r.Setup[4]=uint8_t(i?5:4);r.Mask[0]=r.Mask[1]=r.Mask[2]=r.Mask[3]=r.Mask[4]=r.Mask[5]=255;r.Action=VdReply;r.DataOffset=uint32_t(cursor);r.DataLength=uint32_t(payload.size());std::memcpy(b.data()+h.RulesOffset+i*sizeof(r),&r,sizeof(r));std::memcpy(b.data()+cursor,payload.data(),payload.size());cursor+=payload.size();}
 return b;
}
}
