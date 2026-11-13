# USB-over-IP-SDK PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module USBoverIPSDK -Scope CurrentUser`.

```powershell
$client=Connect-USBoverIPSDKSDK
try { Invoke-USBoverIPSDKSDK -Client $client -Operation client_status } finally { Disconnect-USBoverIPSDKSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
