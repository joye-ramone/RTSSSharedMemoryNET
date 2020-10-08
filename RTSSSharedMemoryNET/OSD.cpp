// This is the main DLL file.

#include "stdafx.h"

#include "Osd.h"

#define TICKS_PER_MICROSECOND 10
#define RTSS_VERSION(x, y) ((x << 16) + y)

namespace RTSSSharedMemoryNET {

    OSD::OSD(String^ entryName)
    {
        if( String::IsNullOrWhiteSpace(entryName) )
            throw gcnew ArgumentException("Entry name cannot be null, empty, or whitespace", "entryName");

        if (entryName->Length > 255)
            throw gcnew ArgumentException("Entry name exceeds max length of 255 when converted to ANSI", "entryName");

        m_entryName = (LPCSTR)Marshal::StringToHGlobalAnsi(entryName).ToPointer();

        //just open/close to make sure RTSS is working
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //start at either our previously used slot, or the top
        for (DWORD i = (m_osdSlot == 0 ? 1 : m_osdSlot); i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)((LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize));

            //if we need a new slot and this one is unused, claim it
            if (m_osdSlot == 0 && !strlen(pEntry->szOSDOwner))
            {
                m_osdSlot = i;
                strcpy_s(pEntry->szOSDOwner, m_entryName);
            }

            //if this is our slot
            if (strcmp(pEntry->szOSDOwner, m_entryName) == 0)
            {
                break;
            }

            //in case we lost our previously used slot or something, let's start over
            if (m_osdSlot != 0)
            {
                m_osdSlot = 0;
                i = 1;
            }
        }

        closeSharedMemory(hMapFile, pMem);

        m_osdSlot = 0;
        m_disposed = false;
    }

    OSD::~OSD()
    {
        if( m_disposed )
            return;

        //delete managed, if any

        this->!OSD();
        m_disposed = true;
    }

    OSD::!OSD()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //find entries and zero them out
        for(DWORD i=1; i < pMem->dwOSDArrSize; i++)
        {
            //calc offset of entry
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)( (LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize) );

            if( strcmp(pEntry->szOSDOwner, m_entryName) == 0 )
            {
                SecureZeroMemory(pEntry, pMem->dwOSDEntrySize); //won't get optimized away
                pMem->dwOSDFrame++; //forces OSD update
            }
        }

        closeSharedMemory(hMapFile, pMem);
        Marshal::FreeHGlobal(IntPtr((LPVOID)m_entryName));
    }

// push managed state on to stack and set unmanaged state
#pragma managed(push, off)

    DWORD InterlockedBitTestAndSetLocal(LPRTSS_SHARED_MEMORY pMem) {
        DWORD dwBusy = InterlockedBitTestAndSet(&pMem->dwBusy, 0);
        return dwBusy;
    }

#pragma managed(pop)

    void OSD::Update(String^ text)
    {
        if( text == nullptr )
            throw gcnew ArgumentNullException("text");

        if (text->Length > 4095)
            throw gcnew ArgumentException("Text exceeds max length of 4095 when converted to ANSI", "text");

        LPCSTR lpText = (LPCSTR)Marshal::StringToHGlobalAnsi(text).ToPointer();

        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //start at either our previously used slot, or the top
        for(DWORD i=(m_osdSlot == 0 ? 1 : m_osdSlot); i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)( (LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize) );

            //if we need a new slot and this one is unused, claim it
            if( m_osdSlot == 0 && !strlen(pEntry->szOSDOwner) )
            {
                m_osdSlot = i;
                strcpy_s(pEntry->szOSDOwner, m_entryName);
            }

            //if this is our slot
            if( strcmp(pEntry->szOSDOwner, m_entryName) == 0 )
            {
                //use extended text slot for v2.7 and higher shared memory, it allows displaying 4096 symbols instead of 256 for regular text slot
                if (pMem->dwVersion >= RTSS_VERSION(2, 7))
                {
                    //OSD locking is supported on v2.14 and higher shared memory
                    if (pMem->dwVersion >= RTSS_VERSION(2, 14))
                    {
                        //bit 0 of this variable will be set if OSD is locked by renderer and cannot be refreshed
                        //at the moment

                        //DWORD dwBusy = InterlockedBitTestAndSet(&pMem->dwBusy, 0);
                        DWORD dwBusy = InterlockedBitTestAndSetLocal(pMem);

                        if (!dwBusy)
                        {
                            strncpy_s(pEntry->szOSDEx, sizeof(pEntry->szOSDEx), lpText, sizeof(pEntry->szOSDEx) - 1);

                            pMem->dwBusy = 0;
                        }
                    }
                    else
                    {
                        strncpy_s(pEntry->szOSDEx, lpText, sizeof(pEntry->szOSDEx) - 1);
                    }
                }
                else
                {
                    strncpy_s(pEntry->szOSD, lpText, sizeof(pEntry->szOSD) - 1);
                }

                pMem->dwOSDFrame++; //forces OSD update

                break;
            }

            //in case we lost our previously used slot or something, let's start over
            if( m_osdSlot != 0 )
            {
                m_osdSlot = 0;
                i = 1;
            }
        }

        closeSharedMemory(hMapFile, pMem);
        Marshal::FreeHGlobal(IntPtr((LPVOID)lpText));
    }

    DWORD OSD::EmbedGraph(DWORD dwOffset, array<FLOAT>^ lpBuffer, DWORD dwBufferPos, LONG dwWidth, LONG dwHeight, LONG dwMargin, FLOAT fltMin, FLOAT fltMax, EMBEDDED_OBJECT_GRAPH dwFlags)
    {
        DWORD dwResult = 0;

        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //start at either our previously used slot, or the top
        for (DWORD i = (m_osdSlot == 0 ? 1 : m_osdSlot); i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)((LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize));

            //if we need a new slot and this one is unused, claim it
            if (m_osdSlot == 0 && !strlen(pEntry->szOSDOwner))
            {
                m_osdSlot = i;
                strcpy_s(pEntry->szOSDOwner, m_entryName);
            }

            //if this is our slot
            if (strcmp(pEntry->szOSDOwner, m_entryName) == 0)
            {
                //embedded graphs are supported for v2.12 and higher shared memory
                if (pMem->dwVersion >= RTSS_VERSION(2, 12))
                {
                    //validate embedded object offset and size and ensure that we don't overrun the buffer
                    if (dwOffset + sizeof(RTSS_EMBEDDED_OBJECT_GRAPH) + lpBuffer->Length * sizeof(FLOAT) > sizeof(pEntry->buffer))
                    {
                        closeSharedMemory(hMapFile, pMem);

                        return 0;
                    }

                    //get pointer to object in buffer
                    LPRTSS_EMBEDDED_OBJECT_GRAPH lpGraph = (LPRTSS_EMBEDDED_OBJECT_GRAPH)(pEntry->buffer + dwOffset);

                    lpGraph->header.dwSignature = RTSS_EMBEDDED_OBJECT_GRAPH_SIGNATURE;
                    lpGraph->header.dwSize = sizeof(RTSS_EMBEDDED_OBJECT_GRAPH) + lpBuffer->Length * sizeof(FLOAT);
                    lpGraph->header.dwWidth = dwWidth;
                    lpGraph->header.dwHeight = dwHeight;
                    lpGraph->header.dwMargin = dwMargin;
                    lpGraph->dwFlags = (DWORD)dwFlags;
                    lpGraph->fltMin = fltMin;
                    lpGraph->fltMax = fltMax;
                    lpGraph->dwDataCount = lpBuffer->Length;

                    if (lpBuffer->Length > 0)
                    {
                        for (DWORD dwPos = 0; dwPos < lpBuffer->Length; dwPos++)
                        {
                            FLOAT fltData = lpBuffer[dwBufferPos];

                            lpGraph->fltData[dwPos] = fltData;

                            dwBufferPos = (dwBufferPos + 1) & (lpBuffer->Length - 1);
                        }
                    }

                    dwResult = lpGraph->header.dwSize;
                }

                break;
            }

            //in case we lost our previously used slot or something, let's start over
            if (m_osdSlot != 0)
            {
                m_osdSlot = 0;
                i = 1;
            }
        }

        closeSharedMemory(hMapFile, pMem);

        return dwResult;
    }


    System::Version^ OSD::Version::get()
    {
        DWORD dwResult = getVersionInternal();
        auto ver = gcnew System::Version(dwResult >> 16, dwResult & 0xFFFF);
        return ver;
    }

    //BOOL bFormatTagsSupported = (dwSharedMemoryVersion >= 0x0002000b);
    ////text format tags are supported for shared memory v2.11 and higher
    //BOOL bObjTagsSupported = (dwSharedMemoryVersion >= 0x0002000c);
    ////embedded object tags are supported for shared memory v2.12 and higher

    DWORD OSD::getVersionInternal()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        DWORD dwResult = pMem->dwVersion;

        closeSharedMemory(hMapFile, pMem);
        return dwResult;
    }

    array<OSDEntry^>^ OSD::GetOSDEntries()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        auto list = gcnew List<OSDEntry^>;

        //include all slots
        for(DWORD i=0; i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)( (LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize) );
            if( strlen(pEntry->szOSDOwner) )
            {
                auto entry = gcnew OSDEntry;
                entry->Owner = Marshal::PtrToStringAnsi(IntPtr(pEntry->szOSDOwner));

                if( pMem->dwVersion >= RTSS_VERSION(2,7) )
                    entry->Text = Marshal::PtrToStringAnsi(IntPtr(pEntry->szOSDEx));
                else
                    entry->Text = Marshal::PtrToStringAnsi(IntPtr(pEntry->szOSD));

                list->Add(entry);
            }
        }

        closeSharedMemory(hMapFile, pMem);
        return list->ToArray();
    }

    array<AppEntry^>^ OSD::GetAppEntries(AppFlags flags)
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        auto list = gcnew List<AppEntry^>;

        //include all slots
        for(DWORD i=0; i < pMem->dwAppArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY)( (LPBYTE)pMem + pMem->dwAppArrOffset + (i * pMem->dwAppEntrySize) );
            if( pEntry->dwProcessID )
            {
                if ((pEntry->dwFlags & (DWORD)flags) != 0)
                {

                    auto entry = gcnew AppEntry;

                    //basic fields
                    entry->ProcessId = pEntry->dwProcessID;
                    entry->Name = Marshal::PtrToStringAnsi(IntPtr(pEntry->szName));
                    entry->Flags = (AppFlags)pEntry->dwFlags;

                    //instantaneous framerate fields
                    entry->InstantaneousTimeStart = timeFromTickCount(pEntry->dwTime0);
                    entry->InstantaneousTimeEnd = timeFromTickCount(pEntry->dwTime1);
                    entry->InstantaneousFrames = pEntry->dwFrames;
                    entry->InstantaneousFrameTime = TimeSpan::FromTicks(pEntry->dwFrameTime * TICKS_PER_MICROSECOND);

                    //framerate stats fields
                    entry->StatFlags = (StatFlags)pEntry->dwStatFlags;
                    entry->StatTimeStart = timeFromTickCount(pEntry->dwStatTime0);
                    entry->StatTimeEnd = timeFromTickCount(pEntry->dwStatTime1);
                    entry->StatFrames = pEntry->dwStatFrames;
                    entry->StatCount = pEntry->dwStatCount;
                    entry->StatFramerateMin = pEntry->dwStatFramerateMin;
                    entry->StatFramerateAvg = pEntry->dwStatFramerateAvg;
                    entry->StatFramerateMax = pEntry->dwStatFramerateMax;

                    if (pMem->dwVersion >= RTSS_VERSION(2, 5))
                    {
                        entry->StatFrameTimeMin = pEntry->dwStatFrameTimeMin;
                        entry->StatFrameTimeAvg = pEntry->dwStatFrameTimeAvg;
                        entry->StatFrameTimeMax = pEntry->dwStatFrameTimeMax;
                        entry->StatFrameTimeCount = pEntry->dwStatFrameTimeCount;

                        //TODO - frametime buffer?
                    }

                    //OSD fields
                    entry->OSDCoordinateX = pEntry->dwOSDX;
                    entry->OSDCoordinateY = pEntry->dwOSDY;
                    entry->OSDZoom = pEntry->dwOSDPixel;
                    entry->OSDFrameId = pEntry->dwOSDFrame;
                    entry->OSDColor = Color::FromArgb(pEntry->dwOSDColor);
                    if (pMem->dwVersion >= RTSS_VERSION(2, 1))
                        entry->OSDBackgroundColor = Color::FromArgb(pEntry->dwOSDBgndColor);

                    //screenshot fields
                    entry->ScreenshotFlags = (ScreenshotFlags)pEntry->dwScreenCaptureFlags;
                    entry->ScreenshotPath = Marshal::PtrToStringAnsi(IntPtr(pEntry->szScreenCapturePath));
                    if (pMem->dwVersion >= RTSS_VERSION(2, 2))
                    {
                        entry->ScreenshotQuality = pEntry->dwScreenCaptureQuality;
                        entry->ScreenshotThreads = pEntry->dwScreenCaptureThreads;
                    }

                    //video capture fields
                    if (pMem->dwVersion >= RTSS_VERSION(2, 2))
                    {
                        entry->VideoCaptureFlags = (VideoCaptureFlags)pEntry->dwVideoCaptureFlags;
                        entry->VideoCapturePath = Marshal::PtrToStringAnsi(IntPtr(pEntry->szVideoCapturePath));
                        entry->VideoFramerate = pEntry->dwVideoFramerate;
                        entry->VideoFramesize = pEntry->dwVideoFramesize;
                        entry->VideoFormat = pEntry->dwVideoFormat;
                        entry->VideoQuality = pEntry->dwVideoQuality;
                        entry->VideoCaptureThreads = pEntry->dwVideoCaptureThreads;
                    }
                    if (pMem->dwVersion >= RTSS_VERSION(2, 4))
                        entry->VideoCaptureFlagsEx = pEntry->dwVideoCaptureFlagsEx;

                    //audio capture fields
                    if (pMem->dwVersion >= RTSS_VERSION(2, 3))
                        entry->AudioCaptureFlags = pEntry->dwAudioCaptureFlags;
                    if (pMem->dwVersion >= RTSS_VERSION(2, 5))
                        entry->AudioCaptureFlags2 = pEntry->dwAudioCaptureFlags2;
                    if (pMem->dwVersion >= RTSS_VERSION(2, 6))
                    {
                        entry->AudioCapturePTTEventPush = pEntry->qwAudioCapturePTTEventPush.QuadPart;
                        entry->AudioCapturePTTEventRelease = pEntry->qwAudioCapturePTTEventRelease.QuadPart;
                        entry->AudioCapturePTTEventPush2 = pEntry->qwAudioCapturePTTEventPush2.QuadPart;
                        entry->AudioCapturePTTEventRelease2 = pEntry->qwAudioCapturePTTEventRelease2.QuadPart;
                    }

                    list->Add(entry);
                }
            }
        }

        closeSharedMemory(hMapFile, pMem);
        return list->ToArray();
    }

    DWORD OSD::GetOSDCount()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        DWORD dwClients = 0;

        //include all slots
        for (DWORD i = 0; i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)((LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize));
            if (strlen(pEntry->szOSDOwner))
            {
                dwClients++;
            }
        }

        closeSharedMemory(hMapFile, pMem);

        return dwClients;
    }

    DWORD OSD::GetAppCount(AppFlags flags)
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        DWORD dwClients = 0;

        //include all slots
        for (DWORD i = 0; i < pMem->dwAppArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY)((LPBYTE)pMem + pMem->dwAppArrOffset + (i * pMem->dwAppEntrySize));
            if (pEntry->dwProcessID)
            {
                if ((pEntry->dwFlags & (DWORD)flags) != 0)
                {
                    dwClients++;
                }
            }
        }

        closeSharedMemory(hMapFile, pMem);

        return dwClients;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void OSD::openSharedMemory(HANDLE* phMapFile, LPRTSS_SHARED_MEMORY* ppMem)
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        try
        {
            hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"RTSSSharedMemoryV2");
            if( !hMapFile )
                THROW_LAST_ERROR();

            pMem = (LPRTSS_SHARED_MEMORY)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if( !pMem )
                THROW_LAST_ERROR();

            if( !(pMem->dwSignature == 'RTSS' && pMem->dwVersion >= RTSS_VERSION(2,0)) )
                throw gcnew System::IO::InvalidDataException("Failed to validate RTSS Shared Memory structure");

            *phMapFile = hMapFile;
            *ppMem = pMem;
        }
        catch(...)
        {
            closeSharedMemory(hMapFile, pMem);
            throw;
        }
    }

    void OSD::closeSharedMemory(HANDLE hMapFile, LPRTSS_SHARED_MEMORY pMem)
    {
        if( pMem )
            UnmapViewOfFile(pMem);

        if( hMapFile )
            CloseHandle(hMapFile);
    }

    DateTime OSD::timeFromTickCount(DWORD ticks)
    {
        return DateTime::Now - TimeSpan::FromMilliseconds(ticks);
    }
}