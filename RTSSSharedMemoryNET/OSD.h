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

        enum class EMBEDDED_OBJECT_GRAPH : DWORD
        {
            FLAG_FILLED     = RTSS_EMBEDDED_OBJECT_GRAPH_FLAG_FILLED,
            FLAG_FRAMERATE  = RTSS_EMBEDDED_OBJECT_GRAPH_FLAG_FRAMERATE,
            FLAG_FRAMETIME  = RTSS_EMBEDDED_OBJECT_GRAPH_FLAG_FRAMETIME,
            FLAG_BAR        = RTSS_EMBEDDED_OBJECT_GRAPH_FLAG_BAR,
            FLAG_BGND       = RTSS_EMBEDDED_OBJECT_GRAPH_FLAG_BGND
        };

        OSD(String^ entryName);
        ~OSD();
        !OSD();

        void Update(String^ text);

        DWORD EmbedGraph(DWORD dwOffset, array<FLOAT>^ lpBuffer, DWORD dwBufferPos, LONG dwWidth, LONG dwHeight, LONG dwMargin, FLOAT fltMin, FLOAT fltMax, EMBEDDED_OBJECT_GRAPH dwFlags);

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
