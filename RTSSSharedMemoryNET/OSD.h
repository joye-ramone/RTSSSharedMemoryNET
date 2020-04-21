// Osd.h

#pragma once

#include "Structs.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace RTSSSharedMemoryNET {

    public ref class OSD
    {
    private:
        LPCSTR m_entryName;
        DWORD m_osdSlot;
        bool m_disposed;

    public:
        OSD(String^ entryName);
        ~OSD();
        !OSD();

        void Update(String^ text);

        static property System::Version^ Version
        {
            System::Version^ get();
        }

        static array<OSDEntry^>^ GetOSDEntries();
        static array<AppEntry^>^ GetAppEntries();

    private:
        static DWORD getVersionInternal();

        static void openSharedMemory(HANDLE* phMapFile, LPRTSS_SHARED_MEMORY* ppMem);
        static void closeSharedMemory(HANDLE hMapFile, LPRTSS_SHARED_MEMORY pMem);

        static DateTime timeFromTickCount(DWORD ticks);
    };

    LPCWSTR MBtoWC(const char* str);
}
