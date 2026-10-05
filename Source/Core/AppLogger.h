#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <juce_core/juce_core.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <windows.h>

namespace ABDMS2000
{

class AppLogger
{
  public:
    static void log(const juce::String &message)
    {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[64];
        snprintf(timeBuf,
                 sizeof(timeBuf),
                 "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                 st.wYear,
                 st.wMonth,
                 st.wDay,
                 st.wHour,
                 st.wMinute,
                 st.wSecond,
                 st.wMilliseconds);

        std::string fullStr = std::string(timeBuf) + message.toRawUTF8() + "\n";
        const char *utf8 = fullStr.c_str();

        OutputDebugStringA(utf8);
        std::cout << utf8;
        std::cout.flush();

        // 3. Write to log file in fixed well-known paths
        static const char *const logPaths[] = {"standalone_debug.log",
                                               "D:\\desarrollos\\ABDSynths\\ABDMS2000\\standalone_debug.log",
                                               "C:\\Users\\ajaba\\AppData\\Local\\Temp\\ABDMS2000_debug.log"};

        for (const char *path : logPaths)
        {
            FILE *f = nullptr;
            if (fopen_s(&f, path, "a") == 0 && f != nullptr)
            {
                fputs(utf8, f);
                fflush(f);
                fclose(f);
            }
        }
    }
};

#define ABD_LOG(msg) ::ABDMS2000::AppLogger::log(msg)

}  // namespace ABDMS2000
#pragma once
#include <juce_core/juce_core.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace ABDMS2000
{

class AppLogger
{
  public:
    static void log(const juce::String &message, const char *file, int line)
    {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream timeStream;
        timeStream << std::put_time(std::localtime(&now_time), "[%Y-%m-%d %H:%M:%S") << "." << std::setfill('0')
                   << std::setw(3) << now_ms.count() << "] ";

        std::string fullStr = timeStream.str() + message.toRawUTF8() + " [" + file + ":" + std::to_string(line) + "]\n";
        const char *utf8 = fullStr.c_str();

        std::cout << utf8;
        std::cout.flush();

        static const std::vector<std::string> logPaths = {
            "standalone_debug.log",
            "D:\\\\desarrollos\\\\ABDSynths\\\\ABDMS2000\\\\standalone_debug.log",
            "C:\\\\Users\\\\ajaba\\\\AppData\\\\Local\\\\Temp\\\\ABDMS2000_debug.log"};

        for (const auto &path : logPaths)
        {
            try
            {
                std::filesystem::path logPath(path);
                if (!std::filesystem::exists(logPath.parent_path()))
                {
                    std::filesystem::create_directories(logPath.parent_path());
                }

                std::ofstream logFile(logPath, std::ios::app);
                if (logFile.is_open())
                {
                    logFile << utf8;
                    logFile.flush();
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error writing to log file " << path << ": " << e.what() << std::endl;
            }
        }
    }
};

#define ABD_LOG(msg) ::ABDMS2000::AppLogger::log(msg, __FILE__, __LINE__)

}  // namespace ABDMS2000
