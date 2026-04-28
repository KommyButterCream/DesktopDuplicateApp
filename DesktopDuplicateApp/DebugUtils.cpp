#include "DebugUtils.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <dxgidebug.h>
#include <dxgi1_3.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")

void ReportLiveObjects()
{
#if defined(_DEBUG)
    IDXGIDebug1* debug = nullptr;

    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
    {
        debug->ReportLiveObjects(
            DXGI_DEBUG_ALL,
            DXGI_DEBUG_RLO_ALL
        );
        debug->Release();
    }
#endif
}