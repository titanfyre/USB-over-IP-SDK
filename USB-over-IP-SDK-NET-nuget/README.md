# USB-over-IP-SDK .NET8 SDK

Package ID `USBoverIPSDK.SDK`. ```csharp
using var client=new TitanCSA.Products.USBoverIPSDK.Client();
string statusJson=client.Call("client_status", "{}");
```

Client implements IDisposable; Call takes raw argument JSON object and returns result JSON. NativeException exposes Code. UInt64 fields remain decimal strings. [API](docs/API.md). Windows x64 and separately installed matching driver required.
