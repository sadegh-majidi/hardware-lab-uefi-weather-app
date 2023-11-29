#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>

#define BUFFER_SIZE 0x100000


BOOLEAN gRequestCallbackComplete = FALSE;
BOOLEAN gResponseCallbackComplete = FALSE;

VOID EFIAPI RequestCallback(IN EFI_EVENT Event, IN VOID *Context) {
    gRequestCallbackComplete = TRUE;
}

VOID EFIAPI ResponseCallback(IN EFI_EVENT Event, IN VOID *Context) {
    gResponseCallbackComplete = TRUE;
}

/**
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.  
  @param[in] SystemTable    A pointer to the EFI System Table.
  
  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
    EFI_STATUS Status;
    EFI_HTTP_PROTOCOL *Http = NULL;
    EFI_SERVICE_BINDING_PROTOCOL *ServiceBinding = NULL;
    EFI_HANDLE Handle = NULL;
    EFI_HTTP_CONFIG_DATA ConfigData;
    EFI_HTTPv4_ACCESS_POINT IPv4Node;
    EFI_HTTP_REQUEST_DATA RequestData;
    //EFI_HTTP_HEADER RequestHeader;
    EFI_HTTP_MESSAGE RequestMessage;
    EFI_HTTP_TOKEN RequestToken;
    EFI_HTTP_RESPONSE_DATA ResponseData;
    EFI_HTTP_MESSAGE ResponseMessage;
    EFI_HTTP_TOKEN ResponseToken;
    UINT8 *Buffer;
    EFI_TIME Baseline;
    EFI_TIME Current;
    UINTN Timer;

    // Locate the HTTP protocol
    Status = gBS->AllocatePool(EfiBootServicesData, BUFFER_SIZE, (VOID **)&Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate pool: %r\n", Status);
        return Status;
    }

    Status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (VOID **)&ServiceBinding);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate binding protocol: %r\n", Status);
        return Status;
    }

    Status = ServiceBinding->CreateChild(ServiceBinding, &Handle);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create child: %r\n", Status);
        return Status;
    }

    Status = gBS->HandleProtocol(Handle, &gEfiHttpProtocolGuid, (VOID **)&Http);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to handle HTTP protocol: %r\n", Status);
        return Status;
    }

    ConfigData.HttpVersion = HttpVersion11;
    ConfigData.TimeOutMillisec = 0;
    ConfigData.LocalAddressIsIPv6 = FALSE;

    ZeroMem(&IPv4Node, sizeof(IPv4Node));
    IPv4Node.UseDefaultAddress = TRUE;
    IPv4Node.LocalPort = 6349;
    ConfigData.AccessPoint.IPv4Node = &IPv4Node;

    Status = Http->Configure(Http, &ConfigData);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure HTTP driver: %r\n", Status);
        return Status;
    }

    RequestData.Url = L"http://amirmahdikousheshi.ir/";
    RequestData.Method = HttpMethodGet;

    RequestMessage.Data.Request = &RequestData;
    RequestMessage.HeaderCount = 0;
    RequestMessage.Headers = NULL;
    RequestMessage.BodyLength = 0;
    RequestMessage.Body = NULL;
    
    RequestToken.Event = NULL;
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RequestCallback, NULL, &RequestToken.Event);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create event: %r\n", Status);
        return Status;
    }

    RequestToken.Status = EFI_SUCCESS;
    RequestToken.Message = &RequestMessage;
    gRequestCallbackComplete = FALSE;

    Status = Http->Request(Http, &RequestToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to send HTTP request: %r\n", Status);
        return Status;
    }

    Status = gRT->GetTime(&Baseline, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get baseline time: %r\n", Status);
        return Status;
    }

    for (Timer = 0; !gRequestCallbackComplete && Timer < 10;) {
        Http->Poll(Http);
	if (!EFI_ERROR(gRT->GetTime(&Current, NULL)) && Current.Second != Baseline.Second) {
	    Baseline = Current;
	    ++Timer;
	}
    }

    if (gRequestCallbackComplete) {
        Status = Http->Cancel(Http, &RequestToken);
	Print(L"Request timed out.");
	return Status;
    }

    ResponseData.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    ResponseMessage.Data.Response = &ResponseData;
    ResponseMessage.HeaderCount = 0;
    ResponseMessage.Headers = NULL;
    ResponseMessage.BodyLength = BUFFER_SIZE;
    ResponseMessage.Body = Buffer;

    ResponseToken.Event = NULL;
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, ResponseCallback, &ResponseToken, &ResponseToken.Event);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create response event: %r\n", Status);
        return Status;
    }

    ResponseToken.Status = EFI_SUCCESS;
    ResponseToken.Message = &ResponseMessage;
    gResponseCallbackComplete = FALSE;

    Status = Http->Response(Http, &ResponseToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to receive HTTP response: %r\n", Status);
        return Status;
    }

    Status = gRT->GetTime(&Baseline, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get baseline time for response: %r\n", Status);
        return Status;
    }

    for (Timer = 0; !gResponseCallbackComplete && Timer < 10;) {
        Http->Poll(Http);
        if (!EFI_ERROR(gRT->GetTime(&Current, NULL)) && Current.Second != Baseline.Second) {
            Baseline = Current;
            ++Timer;
        }
    }

    if (gResponseCallbackComplete) {
        Status = Http->Cancel(Http, &ResponseToken);
	Print(L"Response timed out.");
        return Status;
    }

    Print(L"Response status code: %d\n", ResponseData.StatusCode);


    gBS->Stall (30000000);
    return EFI_SUCCESS;
}

