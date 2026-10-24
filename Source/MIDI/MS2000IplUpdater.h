#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ABDMS2000
{

class HardwareMidiTransport;

/**
 * Korg MS2000 / MS2000R Hardware Firmware Updater & 1 MB Flash Backup ("IPL") Protocol.
 *
 * Implements the host-side IPL SysEx protocol (F0 42 23 58 <cmd> ... F7) dissected from:
 *   - Korg MS2000 v1.07 IPL ROM (H8S/2350 @ 0x4070F0, flash offset 0x000D54..0x001DD6)
 *   - Korg Windows System Update Utility vup_ms2000.exe v1.3
 *   - unknown-technologies fwpatch / fwdump (IPL Command 0x01 Flash Dump at RAM 0x408084)
 *
 * Strictly for Korg MS2000 and MS2000R (original DSP56362 + H8S/2350 hardware).
 * NEVER use with MS2000B / MS2000BR (different hardware/firmware).
 */
namespace ms2kipl
{

using Bytes = std::vector<uint8_t>;

constexpr uint32_t kSysSize = 0x40000;     // 256 KB System OS area (0x000000..0x03FFFF)
constexpr uint32_t kPcmSize = 0x60000;     // 384 KB DWGS PCM area  (0x040000..0x09FFFF)
constexpr uint32_t kUsrSize = 0x60000;     // 384 KB User/Preload   (0x0A0000..0x0FFFFF)
constexpr uint32_t kFlashSize = 0x100000;  // 1 MB Full Flash       (MBM29LV800BA)
constexpr uint32_t kBlockSize = 0x2000;    // 8 KB per IPL Data Block (cmd 0x04)

// Official Korg MS2000 v1.07 system updater file (x811v107.sys) SHA-1
constexpr const char *kV107Sha1 = "945a75555fcd3f17d7ad9f15d38a431869a07bb8";
// Patched v1.07 system image with IPL cmd 0x01 Flash Dump handler at 0x408084
constexpr const char *kDumpSysSha1 = "cd85d579969411b8cd55a6ccd83690247b51c7ca";

enum class Type : uint8_t
{
    SYS = 0,  // 256 KB (32 blocks of 8 KB)
    PCM = 1,  // 384 KB (48 blocks of 8 KB)
    USR = 2,  // 384 KB (48 blocks of 8 KB)
    ALL = 3   // 1 MB   (128 blocks of 8 KB)
};

/** Transfer rate pacing modes matching Korg vup_ms2000.exe configuration. */
enum class TransferRate : int
{
    VeryFast = 0,  // Full 9372-byte block frame at once, 20 ms inter-block wait
    Fast = 1,      // 512-byte chunks with 15 ms pacing
    Slow = 2,      // 256-byte chunks with 30 ms pacing
    VerySlow = 3   // 256-byte chunks with 60 ms pacing
};

uint32_t typeSize(Type t) noexcept;

std::string sha1Hex(const uint8_t *data, size_t n);
inline std::string sha1Hex(const Bytes &b)
{
    return sha1Hex(b.data(), b.size());
}

/**
 * Builds the flash-dump-enabled v1.07 system image from official x811v107.sys:
 * injects the 366-byte H8S/2350 dump routine at file 0x1DD8 (RAM 0x408084),
 * updates IPL copy count at 0x1CCE, hooks cmd_table[1] at 0x1CE0, and rebalances
 * the 16-bit big-endian word checksum at 0x1FFE so the 256 KB sum remains 0x0000.
 */
bool makeDumpSys(const Bytes &v107, Bytes &out, std::string &err);
std::string validateImage(Type t, const Bytes &image);
bool isOfficialV107(const Bytes &image);
bool isDumpSys(const Bytes &image);

// ---- Host -> MS2000 IPL SysEx Messages ----
Bytes msgVersion();
Bytes msgSetType(Type t);
Bytes msgBlock(uint8_t blockInType, const uint8_t *data8k);
Bytes msgDump(uint32_t addr, uint32_t size);

// ---- MS2000 IPL -> Host Replies ----
struct Version
{
    uint16_t sys = 0;  // major << 8 | minor (v1.07 = 0x0107)
    uint16_t pcm = 0;  // major << 8 | minor (v1.00 = 0x0100)
    uint16_t usr = 0;  // major << 8 | minor (v1.00 = 0x0100)
};

bool parseVersion(const Bytes &sx, Version &v);
int parseStatus(const Bytes &sx);  // 0 = OK, 1..5 = error, -1 = not a status reply
std::string statusText(int code);
std::string statusLcdCode(int code);
bool decodeDump(const Bytes &sx, uint32_t size, Bytes &out);
std::string versionText(uint16_t v);

/** Abstract MIDI transport for IPL sessions (testable with mock or real HardwareMidiTransport). */
struct Transport
{
    virtual ~Transport() = default;
    virtual bool send(const Bytes &msg, TransferRate rate = TransferRate::VeryFast) = 0;
    virtual bool receive(Bytes &sysex, int timeoutMs) = 0;
    virtual void wait(int ms) = 0;
};

struct ProgressUpdate
{
    int done = 0;
    int total = 0;
    std::string stage;
    std::string lcdLine1;
    std::string lcdLine2;
};

using ProgressCallback = std::function<bool(const ProgressUpdate &update)>;

struct Result
{
    bool ok = false;
    int statusCode = 0;
    std::string error;
    std::string lcdLine1;
    std::string lcdLine2;
};

Result getVersion(Transport &t, Version &v, int timeoutMs = 2500);
Result install(Transport &t, Type type, const Bytes &image, TransferRate rate, const ProgressCallback &progress);
Result dump(Transport &t, uint32_t addr, uint32_t size, Bytes &out, const ProgressCallback &progress, int retries = 3);

}  // namespace ms2kipl

/**
 * Asynchronous controller owned by BridgeActions / Plugin to run IPL operations
 * on a background worker thread and emit progress/state events to the WebUI.
 */
class MS2000IplUpdater
{
  public:
    using EventCallback = std::function<void(const std::string &eventType, const std::string &jsonPayload)>;

    MS2000IplUpdater();
    ~MS2000IplUpdater();

    const ms2kipl::Bytes &getEmbeddedV107() const noexcept
    {
        return embeddedV107_;
    }
    const ms2kipl::Bytes &getEmbeddedDumpSys() const noexcept
    {
        return embeddedDumpSys_;
    }
    bool isEmbeddedFirmwareValid() const noexcept
    {
        return embeddedValid_;
    }
    std::string getEmbeddedSha1() const noexcept
    {
        return embeddedSha1_;
    }
    std::string getDumpSysSha1() const noexcept
    {
        return dumpSysSha1_;
    }

    bool isBusy() const noexcept
    {
        return busy_.load();
    }
    void requestCancel() noexcept
    {
        cancel_.store(true);
    }

    /** Runs a quick version check (or silent poll) on a background thread. */
    void startCheckVersion(HardwareMidiTransport &transport, bool silentPoll, EventCallback emit);

    /** Installs either the official v1.07 (`installDumpPatch=false`) or the dump-enabled v1.07 (`true`). */
    void startInstallSystem(HardwareMidiTransport &transport,
                            bool installDumpPatch,
                            ms2kipl::TransferRate rate,
                            EventCallback emit);

    /** Reads the flash (either full 1 MB or 256 KB SYS) over MIDI using cmd 0x01. */
    void startFlashDump(HardwareMidiTransport &transport, uint32_t sizeBytes, EventCallback emit);

  private:
    void joinWorker();

    ms2kipl::Bytes embeddedV107_;
    ms2kipl::Bytes embeddedDumpSys_;
    std::string embeddedSha1_;
    std::string dumpSysSha1_;
    bool embeddedValid_ = false;

    std::atomic<bool> busy_ {false};
    std::atomic<bool> cancel_ {false};
    std::thread worker_;
    std::mutex workerMutex_;
};

}  // namespace ABDMS2000
