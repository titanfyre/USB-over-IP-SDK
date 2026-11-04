#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace UsbIp {
struct Device { std::wstring name,path; };
struct Pairing { std::string address,fingerprint,secret; std::uint16_t port=0; };
struct Status { bool active=false,connected=false;std::uint64_t requests=0,completed=0,failed=0;std::string detail; };
// Only interfaces already using Microsoft's WinUSB driver are enumerated.
// No physical device driver, USB filter, firewall or certificate store is changed.
std::vector<Device> Devices();
// Empty string means success. Unlocks product 9 in this boot; errors are English.
std::string Verification(std::string_view key);
class Exporter {
 struct State;std::unique_ptr<State> state_;mutable std::recursive_mutex lifecycle_;
public:
 Exporter();~Exporter();Exporter(const Exporter&)=delete;Exporter& operator=(const Exporter&)=delete;
 // Exact interface path, explicit numeric bind address, one authenticated peer.
 // Default is loopback. Give only the intended recipient the pairing details.
 Pairing Start(const std::wstring& devicePath,std::string_view bind="127.0.0.1",std::uint16_t port=0);
 Status GetStatus() const;
 void Stop() noexcept;
};
class Client {
 struct State;std::unique_ptr<State> state_;mutable std::recursive_mutex lifecycle_;
public:
 Client();~Client();Client(const Client&)=delete;Client& operator=(const Client&)=delete;
 // Creates one owned virtual device only after TLS pin + session-secret checks.
 void Connect(const Pairing& pairing);
 Status GetStatus() const;
 void Stop() noexcept;
};
}
