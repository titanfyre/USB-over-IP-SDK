#include "../SDK/Transport.h"
#include "../SDK/Wire.h"
#include "../SDK/DescriptorValidation.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace UsbIp;
namespace {unsigned checks=0;void Check(bool value){++checks;if(!value)throw std::runtime_error("A protocol/transport assertion failed at check "+std::to_string(checks));}
void Tls(){
 TitanTransport::Runtime network;TitanTransport::Identity identity;uint16_t port=0;SOCKET listen=TitanTransport::Listen("127.0.0.1",0,&port);std::exception_ptr failure;
 std::thread server([&]{try{SOCKET accepted=accept(listen,nullptr,nullptr);if(accepted==INVALID_SOCKET)throw std::runtime_error("Loopback accept failed.");TitanTransport::Channel channel(accepted,identity);std::vector<uint8_t> bytes(70001);channel.Receive(bytes.data(),bytes.size());channel.Send(bytes.data(),bytes.size());}catch(...){failure=std::current_exception();}});
 try{TitanTransport::Channel channel(TitanTransport::Connect("127.0.0.1",port),identity.Fingerprint());std::vector<uint8_t> sent(70001);for(size_t i=0;i<sent.size();++i)sent[i]=uint8_t(i*17);channel.Send(sent.data(),sent.size());std::vector<uint8_t> received(sent.size());channel.Receive(received.data(),received.size());Check(sent==received);}catch(...){closesocket(listen);server.join();throw;}
 closesocket(listen);server.join();if(failure)std::rethrow_exception(failure);
 bool refused=false;try{SOCKET invalid=TitanTransport::Connect("not-an-ip",1);closesocket(invalid);}catch(...){refused=true;}Check(refused);
}
}
int main(){try{
 Wire::Header h;h.type=Wire::Hello;h.size=32;Check(Wire::Valid(h));for(unsigned i=0;i<200000;++i){auto copy=h;copy.size=i;Check(Wire::Valid(copy)==(i==32));}
 h={};h.type=Wire::Request;h.id=1;h.endpoint=0x81;h.capacity=4096;Check(Wire::Valid(h));h.size=1;Check(!Wire::Valid(h));h.size=0;h.capacity=4097;Check(!Wire::Valid(h));h.capacity=10;h.endpoint=0x91;Check(!Wire::Valid(h));
 h={};h.type=Wire::Request;h.id=2;h.endpoint=1;h.capacity=4;h.size=4;Check(Wire::Valid(h));h.size=3;Check(!Wire::Valid(h));
 h={};h.type=Wire::Request;h.id=3;h.flags=1;h.capacity=4;h.setup[0]=0xc0;h.setup[6]=4;Check(Wire::Valid(h));h.setup[0]=0x80;Check(!Wire::Valid(h));
 std::vector<Wire::Descriptor> ds={
  {0,0,{18,1,0,2,0,0,0,64,9,0x12,0x79,0x7a,0,1,0,0,0,1}},
  {0,0,{9,2,32,0,1,1,0,128,50,9,4,0,0,2,255,0,0,0,7,5,0x81,2,64,0,0,7,5,1,2,64,0,0}},
  {0xee,0,{18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,0x5b,0}}
 };
 auto blob=Wire::DescriptionBlob(ds,VdFullSpeed);VD_LAYOUT layout{};Check(VdValidateCreate(blob.data(),uint32_t(blob.size()),&layout)==0);Check(layout.EndpointCount==2);
 const auto* description=reinterpret_cast<const VD_CREATE*>(blob.data());
 const auto* rules=reinterpret_cast<const VD_CONTROL_RULE*>(blob.data()+description->RulesOffset);
 Check(description->RuleCount==2);
 Check(rules[0].Setup[0]==0xc0&&rules[0].Setup[1]==0x5b&&rules[0].Setup[4]==4);
 Check(rules[1].Setup[0]==0xc1&&rules[1].Setup[1]==0x5b&&rules[1].Setup[4]==5);
 for(unsigned rule=0;rule<2;++rule)for(unsigned byte=0;byte<6;++byte)Check(rules[rule].Mask[byte]==255);
 auto fixture=ds;fixture[1].data={9,2,39,0,1,1,0,128,50,9,4,0,0,3,255,0,0,0,7,5,0x81,2,64,0,0,7,5,1,2,64,0,0,7,5,0x82,3,8,0,1};
 auto completeFixture=Wire::DescriptionBlob(fixture,VdFullSpeed);Check(VdValidateCreate(completeFixture.data(),uint32_t(completeFixture.size()),&layout)==0&&layout.EndpointCount==3);
 auto highFixture=fixture;for(unsigned offset:{22u,29u}){highFixture[1].data[offset]=0;highFixture[1].data[offset+1]=2;}
 auto highBlob=Wire::DescriptionBlob(highFixture,VdHighSpeed);Check(VdValidateCreate(highBlob.data(),uint32_t(highBlob.size()),&layout)==0&&layout.EndpointCount==3);
 auto inconsistent=Wire::DescriptionBlob(fixture,VdHighSpeed);Check(VdValidateCreate(inconsistent.data(),uint32_t(inconsistent.size()),&layout)!=0);
 for(size_t length=0;length<blob.size();++length)Check(VdValidateCreate(blob.data(),uint32_t(length),&layout)!=0);
 auto original=blob;auto* create=reinterpret_cast<VD_CREATE*>(blob.data());create->Flags=0;Check(VdValidateCreate(blob.data(),uint32_t(blob.size()),&layout)!=0);blob=original;
 const auto* descriptors=reinterpret_cast<const VD_DESCRIPTOR*>(blob.data()+sizeof(VD_CREATE));blob[descriptors[1].Offset+14]=3;Check(VdValidateCreate(blob.data(),uint32_t(blob.size()),&layout)!=0);
 auto props=Wire::ExtendedProperties();Check(props.size()>100&&props[0]==uint8_t(props.size()));auto secret=TitanTransport::RandomSecret();Check(TitanTransport::IsSecret(secret));Check(TitanTransport::ConstantTimeEqual(secret,secret));auto changed=secret;changed[0]=changed[0]=='0'?'1':'0';Check(!TitanTransport::ConstantTimeEqual(secret,changed));Tls();
 std::cout<<checks<<" protocol/bounds/descriptor/TLS checks passed; no driver, USB device, remote screen or input was opened.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
