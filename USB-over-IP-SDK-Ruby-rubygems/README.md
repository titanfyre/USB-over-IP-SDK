# USB-over-IP-SDK Ruby SDK

`gem install usb-over-ip-sdk`

Windows x64 Ruby3.1+.

```ruby
require "usb_over_ip_sdk"
USBoverIPSDKSDK::Client.open { |client| p client.call("client_status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
