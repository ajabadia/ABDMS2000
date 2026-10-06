#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
    static void log(const juce::String &message, const char *file = __FILE__, int line = __LINE__)
    {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream timeStream;
        timeStream << std::put_time(std::localtime(&now_time), "[%Y-%m-%d %H:%M:%S") << "."
                   << std::setfill('0') << std::setw(3) << now_ms.count() << "] ";

        std::string fullStr = timeStream.str() + message.toRawUTF8() + " [" + file + ":" + std::to_string(line) + "]\n";
        const char *utf8 = fullStr.c_str();

        std::cout << utf8;
        std::cout.flush();
    }
};

#define ABD_LOG(msg) ::ABDMS2000::AppLogger::log(msg, __FILE__, __LINE__)

}  // namespace ABDMS2000
