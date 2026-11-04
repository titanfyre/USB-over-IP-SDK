# Validation and boundaries

This document describes the current signed demo/full implementation and its
release qualification. The separate [demo-only source preview](../driver/README.md)
uses new identities and has compile/link plus ABI-rejection checks only.
The runtime and hardware results below do not qualify that new preview.

USB-over-IP SDK 0.1.1 targets Windows 10 version 2004/build 19041+ x64 and Windows 11 x64. The
new UdeCx driver has its own identity and qualification; prior Vdevice results
do not qualify this driver automatically.

## Offline checks

Normal and AddressSanitizer CTest builds pass 200,451 protocol/descriptor/bounds
and real Schannel-loopback assertions, plus seven read-only installer identity
and signature-refusal checks. Applications, SDK and both installers compile
W4/WX, static CRT, CFG, ASLR and NX. These offline tests open no USB driver or
interface, replace no physical function driver, capture no desktop and inject no
input. Assertion totals are not independent physical hardware test cases.

## Completed guest campaigns

The frozen candidate completed both a normal campaign and a targeted Driver
Verifier campaign in the owned Hyper-V Windows build 19045
(10.0.19045) guest. Each completed installation, its explicit runtime
fixtures, package removal and required normal restarts. The guest was restored
to its original checkpoint and finished Off; host boot/security policy was not
changed. These results cover that candidate and fixture, not every Windows PC.

The original candidate completed its pre-submission tests with guest-only test
signing. The returned Microsoft packages additionally passed kernel-policy trust,
catalog membership and unchanged Authenticode-content checks against that exact
candidate. Both final UI and console setups were inspected as data: their embedded
drivers and applications match the reviewed release files byte for byte.

The returned packages then completed normal and targeted Driver Verifier
campaigns in the owned Windows 10 build 19045 guest with **test signing disabled**.
The four actual console installers performed installation and uninstallation;
the same native functional fixtures, genuine signed full-mode grants and owned
interactive desktop test passed. Exact installed SYS hashes, package removal,
evidence collection and restoration to the original Off guest were verified.
This establishes the documented evaluation scope, not HLK/WHQL certification or
qualification of Windows 11, Secure Boot/HVCI and arbitrary physical hardware.
The application bundle is 0.1.1; signed driver package versions remain 0.1.0.0.

## Demo and activation qualification

The production runtime is configured for one hour per product per Windows boot.
The production functional campaigns **did not wait a full hour**. A separately
pinned private build of the same runtime code used an explicit twelve-second
duration to exercise real expiry, per-product independence, no reset by closing
and reopening handles or repeated Begin, full-mode activation after expiry, and
full-mode persistence beyond another twelve seconds. Products 8 and 10 expired
on their own deadlines and were not unlocked by product 7's activation. Product
9 was already full after its own USB
test and is not described as an unused slot in the lifecycle probe.

The USB short-runtime fixture independently kept a read pending across the real
cutoff, observed completion after expiry and before its independent physical-read
timeout budget, and refused a new export after expiry. Genuine full-mode transfer
recovery followed. Known-IOCTL malformed buffers, restricted callers, limited
handle access, wrong-handle/cross-product proofs, modified signatures, replaced
and consumed nonces, and accepted-proof replay were refused. Five original
broker grants were independently verified against the public P256 key. These
tests are not proof of unbreakable licensing or immunity to kernel compromise.

Normal and AddressSanitizer offline builds also passed the strict shared runtime
installer's 115 identity/trust-refusal checks. License-server HTTP and signature
verification run during activation under a monotonic whole-operation budget.
Data paths retain their local entitlement checks and normal processing costs;
encrypted transports still perform their own TLS work.

## USB fixture scope

Both production guest campaigns passed a real WinUSB exporter, pinned TLS
loopback, the new UdeCx client, byte-compared bulk IN/OUT and interrupt IN,
vendor control requests, cancellation, unauthorized-peer refusal before virtual
child creation, demo/full transfers and owner/child teardown. The only exported
device was the independently signed owned Vdevice WinUSB fixture. Loopback over
a synthetic device does not establish physical USB timing or a two-PC network
benchmark. The separate short-runtime campaign additionally exercised actual
expiry and recovery, as described above.

The pre-signature qualified UdeCx driver input SHA256 is 18dd213ef98c043689f49781c3cde6df3fd8d3d4bbb3d7cb62c932e38c5264c3.
The unsigned qualification runtime input SHA256 is 5ee4e64afeffe527a02b74fc50bdcc7eed4808e6e3ec9a3db028039057c6bf80.

## Defined implementation boundaries

Only one selected vendor-specific WinUSB interface 0, alternate setting 0, with
one USB 2 configuration is supported. Payloads are bounded at 4,096 bytes, at most
32 requests can remain pending, and each endpoint has its own FIFO worker/manual
queue. An OUT URB is acknowledged only after the remote WinUSB operation returns.
Unmatched, oversized or duplicate active request IDs are refused.

The virtual child's optional Microsoft OS discovery metadata binds it to inbox
WinUSB using this SDK's interface GUID. The SDK does not replace a physical
device's function driver. Arbitrary existing vendor-app discovery, full
descriptor fidelity, HID/storage redirection, composite or alternate settings,
USB 3, hubs, isochronous transfers and physical endpoint-reset forwarding are not
claimed. Disconnect/reconnect is the supported initial recovery path.

The encrypted transport uses standard Windows Schannel TLS 1.2 with an exact
SHA-256 peer certificate pin and a separate random 128-bit session secret.
Certificate generation uses a random named current-user Microsoft RSA/AES CSP
key container; normal identity destruction deletes it, while a process crash may
leave that small random key container. Certificates remain in memory and no
certificate is added to a trust store. The pairing secret must reach the intended
peer over a trusted channel. No firewall rule or unattended access is enabled.

Kernel data/IOCTL paths consult the referenced resident runtime's product-9
permission through its bounded internal callback. CNG/HTTPS run during activation,
not per USB transfer. The runtime remains pinned until WDF driver destruction
after device/queue/request callbacks retire. Process close, function-driver reload
and wall-clock changes do not reset the one-hour per-boot deadline.

Full activation removes the demo cutoff for that product in the current boot.
It does not increase the topology/queue limits or assert compatibility beyond
the implemented edition. Revocation affects future activation; accepted boot
rights remain until Windows restarts. No TPM, machine count or anti-kernel-control
guarantee is claimed.

## Remaining qualification

Representative physical WinUSB
hardware, two-PC/Wi-Fi links, Windows 11, Secure Boot/HVCI, sleep/resume,
low-resource simulation and prolonged throughput/latency measurements remain
separate qualification. These tests establish neither universal USB support nor
zero overhead nor WHQL certification. Kernel source for the current signed
implementation with full activation remains excluded. The separate
[demo-only source preview](../driver/README.md) is included in `driver/`. Private
lab probes/grants, signing credentials, PDBs and CABs remain excluded from
public downloads.
