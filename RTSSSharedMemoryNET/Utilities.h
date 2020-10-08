#pragma once

//for easily handling unmanaged to managed errors
#define THROW_HR(hr) System::Runtime::InteropServices::Marshal::ThrowExceptionForHR(hr)
#define THROW_LAST_ERROR() THROW_HR(System::Runtime::InteropServices::Marshal::GetHRForLastWin32Error())
