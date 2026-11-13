/* SPDX-License-Identifier: MIT */
#ifndef USB_OVER_IP_SDK_CSDK_H
#define USB_OVER_IP_SDK_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define USB_OVER_IP_SDK_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define USB_OVER_IP_SDK_CSDK_NOEXCEPT
#endif
#if defined(USB_OVER_IP_SDK_CSDK_BUILD)
#define USB_OVER_IP_SDK_CSDK_API __declspec(dllexport)
#else
#define USB_OVER_IP_SDK_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
USB_OVER_IP_SDK_CSDK_API uint32_t usb_over_ip_sdk_json_open(uint64_t *session) USB_OVER_IP_SDK_CSDK_NOEXCEPT;
USB_OVER_IP_SDK_CSDK_API uint32_t usb_over_ip_sdk_json_close(uint64_t session) USB_OVER_IP_SDK_CSDK_NOEXCEPT;
USB_OVER_IP_SDK_CSDK_API char *usb_over_ip_sdk_json_call(uint64_t session,const char *request,uint32_t length) USB_OVER_IP_SDK_CSDK_NOEXCEPT;
USB_OVER_IP_SDK_CSDK_API void usb_over_ip_sdk_json_free(char *response) USB_OVER_IP_SDK_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef USB_OVER_IP_SDK_CSDK_NOEXCEPT
#endif
