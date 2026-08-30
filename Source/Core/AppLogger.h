#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <juce_core/juce_core.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <cstdio>

namespace ABDMS2000 {

class AppLogger {
public:
    static void log(const juce::String& message) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[64];
        snprintf(timeBuf, sizeof(timeBuf), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        std::string fullStr = std::string(timeBuf) + message.toRawUTF8() + "\n";
        const char* utf8 = fullStr.c_str();

        OutputDebugStringA(utf8);
        std::cout << utf8;
        std::cout.flush();

        // 3. Write to log file in fixed well-known paths
        static const char* const logPaths[] = {
            "standalone_debug.log",
            "D:\\desarrollos\\ABDSynths\\ABDMS2000\\standalone_debug.log",
            "C:\\Users\\ajaba\\AppData\\Local\\Temp\\ABDMS2000_debug.log"
        };

        for (const char* path : logPaths) {
            FILE* f = nullptr;
            if (fopen_s(&f, path, "a") == 0 && f != nullptr) {
                fputs(utf8, f);
                fflush(f);
                fclose(f);
            }
        }
    }
};

#define ABD_LOG(msg) ::ABDMS2000::AppLogger::log(msg)

} // namespace ABDMS2000


