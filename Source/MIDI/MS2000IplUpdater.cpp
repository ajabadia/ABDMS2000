#include "MS2000IplUpdater.h"
#include "../Plugin/HardwareMidiTransport.h"
#include "DWGSBinary.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace ABDMS2000
{

namespace ms2kipl
{

// unknown-technologies/ms2000 fwpatch/fwpatch.asm (GPL-3.0), assembled for H8S/2350 at RAM 0x408084.
// IPL command 0x01 = Flash Dump:
//   Host -> Unit: F0 42 23 58 01 <addr 3x7 + msb> <size 3x7 + msb> F7
//   Unit -> Host: F0 <data: 7 bytes followed by their msb byte>... F7
// 366 bytes, SHA-1: 7bbc825ab8d93bf8da03a54bc646ab0f5e6e9f0b
static constexpr uint8_t kDumpPatch[366] = {
    0x5E, 0x40, 0x81, 0x74, 0x01, 0x00, 0x6D, 0xF0, 0x5E, 0x40, 0x81, 0x74, 0x01, 0x00, 0x6D, 0xF0,
    0x6A, 0x30, 0x00, 0xFF, 0xFF, 0x84, 0x73, 0x60, 0x47, 0xF6, 0x6A, 0x2B, 0x00, 0xFF, 0xFF, 0x85,
    0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84, 0x72, 0x60, 0x7A, 0x01, 0x00, 0x40, 0x81, 0xEA, 0xF0, 0x10,
    0x5E, 0x40, 0x7D, 0x20, 0x01, 0x00, 0x6D, 0x70, 0x01, 0x00, 0x6D, 0x71, 0xFA, 0xF0, 0x5C, 0x00,
    0x00, 0xA2, 0x6A, 0xAA, 0x00, 0xFF, 0xFF, 0x83, 0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84, 0x72, 0x70,
    0x0F, 0x80, 0x58, 0x70, 0x00, 0x6A, 0xFB, 0x07, 0x7A, 0x21, 0x00, 0x10, 0x00, 0x00, 0x58, 0x20,
    0x00, 0x40, 0x6C, 0x1A, 0x73, 0x7A, 0x58, 0x70, 0x00, 0x02, 0x70, 0x72, 0x11, 0x02, 0xEA, 0x7F,
    0x5C, 0x00, 0x00, 0x70, 0x6A, 0xAA, 0x00, 0xFF, 0xFF, 0x83, 0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84,
    0x72, 0x70, 0x1A, 0x0B, 0x58, 0x60, 0x00, 0x16, 0x5C, 0x00, 0x00, 0x58, 0x6A, 0xA2, 0x00, 0xFF,
    0xFF, 0x83, 0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84, 0x72, 0x70, 0x15, 0x22, 0xFB, 0x07, 0x1B, 0x70,
    0x46, 0xB6, 0xAB, 0x07, 0x58, 0x70, 0x00, 0x18, 0x11, 0x02, 0x1A, 0x0B, 0x46, 0xFA, 0x5C, 0x00,
    0x00, 0x32, 0x6A, 0xA2, 0x00, 0xFF, 0xFF, 0x83, 0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84, 0x72, 0x70,
    0xFA, 0xF7, 0x5C, 0x00, 0x00, 0x1E, 0x6A, 0xAA, 0x00, 0xFF, 0xFF, 0x83, 0x6A, 0x38, 0x00, 0xFF,
    0xFF, 0x84, 0x72, 0x70, 0x7A, 0x01, 0x00, 0x40, 0x80, 0x56, 0xF0, 0x10, 0x5E, 0x40, 0x7D, 0x20,
    0x5A, 0x40, 0x74, 0x76, 0x6A, 0x30, 0x00, 0xFF, 0xFF, 0x84, 0x73, 0x70, 0x47, 0xF6, 0x54, 0x70,
    0x1A, 0x91, 0x78, 0x10, 0x6A, 0xA9, 0x00, 0x40, 0x62, 0x4C, 0x0B, 0x71, 0x6A, 0x28, 0x00, 0xFF,
    0xFF, 0x84, 0xE8, 0x38, 0x58, 0x60, 0xF1, 0x26, 0x6A, 0x30, 0x00, 0xFF, 0xFF, 0x84, 0x73, 0x60,
    0x47, 0xEA, 0x6A, 0x28, 0x00, 0xFF, 0xFF, 0x85, 0x6A, 0x38, 0x00, 0xFF, 0xFF, 0x84, 0x72, 0x60,
    0xE8, 0x7F, 0x78, 0x10, 0x6A, 0xA8, 0x00, 0x40, 0x62, 0x4C, 0x0B, 0x71, 0xA9, 0x05, 0x46, 0xCC,
    0x6A, 0x28, 0x00, 0x40, 0x62, 0x50, 0x1A, 0x91, 0x73, 0x08, 0x58, 0x70, 0x00, 0x12, 0x78, 0x10,
    0x6A, 0x20, 0x00, 0x40, 0x62, 0x4D, 0x70, 0x70, 0x78, 0x10, 0x6A, 0xA0, 0x00, 0x40, 0x62, 0x4D,
    0x0B, 0x71, 0x11, 0x08, 0xA9, 0x03, 0x46, 0xE0, 0x1A, 0x91, 0x01, 0x00, 0x78, 0x10, 0x6B, 0x20,
    0x00, 0x40, 0x62, 0x4C, 0x54, 0x70, 0x44, 0x55, 0x4D, 0x50, 0x49, 0x4E, 0x47, 0x00,
};

uint32_t typeSize(Type t) noexcept
{
    switch (t)
    {
        case Type::SYS: return kSysSize;
        case Type::PCM: return kPcmSize;
        case Type::USR: return kUsrSize;
        case Type::ALL: return kFlashSize;
    }
    return 0;
}

// ---- FIPS 180-1 SHA-1 ----
std::string sha1Hex(const uint8_t *data, size_t n)
{
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    auto rol = [](uint32_t x, int s) { return (x << s) | (x >> (32 - s)); };
    std::vector<uint8_t> m(data, data + n);
    const uint64_t bits = static_cast<uint64_t>(n) * 8u;
    m.push_back(0x80);
    while (m.size() % 64 != 56)
        m.push_back(0);
    for (int i = 7; i >= 0; --i)
        m.push_back(static_cast<uint8_t>(bits >> (i * 8)));

    for (size_t off = 0; off < m.size(); off += 64)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32_t>(m[off + 4 * i]) << 24)
                 | (static_cast<uint32_t>(m[off + 4 * i + 1]) << 16)
                 | (static_cast<uint32_t>(m[off + 4 * i + 2]) << 8)
                 | static_cast<uint32_t>(m[off + 4 * i + 3]);
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i)
        {
            uint32_t f = 0, k = 0;
            if (i < 20)
            {
                f = (b & c) | (~b & d);
                k = 0x5A827999u;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = t;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    char s[41];
    for (int i = 0; i < 5; ++i)
        std::snprintf(s + 8 * i, 9, "%08x", h[i]);
    return s;
}

static uint32_t be16(const Bytes &b, size_t o)
{
    return (static_cast<uint32_t>(b[o]) << 8) | b[o + 1];
}
static uint32_t be32(const Bytes &b, size_t o)
{
    return (be16(b, o) << 16) | be16(b, o + 2);
}
static void put16(Bytes &b, size_t o, uint32_t v)
{
    b[o]     = static_cast<uint8_t>(v >> 8);
    b[o + 1] = static_cast<uint8_t>(v);
}
static void put32(Bytes &b, size_t o, uint32_t v)
{
    put16(b, o, v >> 16);
    put16(b, o + 2, v & 0xFFFFu);
}

// v1.07 IPL layout in x811v107.sys:
// Boot launcher at 0x1CB2 copies kIplCount (0x1083) bytes from file 0x0D54 to RAM 0x407000.
// Copy count word is at 0x1CCE; 8-entry 32-bit command table is at 0x1CDC (RAM 0x407F88).
// Main firmware starts at 0x2000; 0x1DD7..0x1FFF is 0xFF padding (553 bytes free).
static constexpr size_t   kIplFile  = 0x0D54;
static constexpr size_t   kCountAt  = 0x1CCE;
static constexpr size_t   kTableAt  = 0x1CDC;
static constexpr size_t   kFirmware = 0x2000;
static constexpr uint32_t kIplCount = 0x1083;
static constexpr uint32_t kIplRam   = 0x407000;
static constexpr uint32_t kTableV107[8] = {
    0x407376, 0x40730E, 0x40737A, 0x40730E, 0x40738C, 0x40730E, 0x40730E, 0x40730E
};
static constexpr size_t kSumWord = 0x1FFE;  // Unused 0xFFFF word at end of boot sector to keep 16-bit word sum == 0

bool makeDumpSys(const Bytes &v107, Bytes &out, std::string &err)
{
    if (v107.size() != kSysSize)
    {
        err = "Not a 256 KB system file";
        return false;
    }
    if (sha1Hex(v107) != kV107Sha1)
    {
        err = "Not KORG's official MS2000 v1.07 system file x811v107.sys (SHA-1 mismatch)";
        return false;
    }
    if (be16(v107, kCountAt) != kIplCount)
    {
        err = "Unexpected IPL copy count at 0x1CCE";
        return false;
    }
    for (int i = 0; i < 8; ++i)
    {
        if (be32(v107, kTableAt + 4 * i) != kTableV107[i])
        {
            err = "Unexpected IPL command table at 0x1CDC";
            return false;
        }
    }
    const size_t at = kIplFile + kIplCount + 1;  // 0x1DD8 (even address -> RAM 0x408084)
    const size_t n  = sizeof(kDumpPatch);
    if (at + n > kSumWord)
    {
        err = "Dump patch does not fit before 0x1FFE";
        return false;
    }
    for (size_t i = kIplFile + kIplCount; i < kFirmware; ++i)
    {
        if (v107[i] != 0xFF)
        {
            err = "Padding space after IPL (0x1DD7..0x1FFF) is not 0xFF";
            return false;
        }
    }
    uint32_t sum = 0;
    for (size_t i = 0; i < kSysSize; i += 2)
        sum += be16(v107, i);
    if ((sum & 0xFFFFu) != 0)
    {
        err = "System image 16-bit word checksum is not 0x0000";
        return false;
    }

    out = v107;
    put16(out, kCountAt, static_cast<uint32_t>(kIplCount + n + 1));
    std::memcpy(out.data() + at, kDumpPatch, n);
    put32(out, kTableAt + 4 * 1, kIplRam + static_cast<uint32_t>(at - kIplFile));
    put16(out, kSumWord, 0);
    sum = 0;
    for (size_t i = 0; i < kSysSize; i += 2)
        sum += be16(out, i);
    put16(out, kSumWord, (0x10000u - (sum & 0xFFFFu)) & 0xFFFFu);
    return true;
}

bool isOfficialV107(const Bytes &image)
{
    return image.size() == kSysSize && sha1Hex(image) == kV107Sha1;
}

bool isDumpSys(const Bytes &image)
{
    return image.size() == kSysSize && sha1Hex(image) == kDumpSysSha1;
}

std::string validateImage(Type t, const Bytes &image)
{
    if (image.size() != typeSize(t))
        return "Wrong file size for update type";

    const bool sysPart = (t == Type::SYS || t == Type::ALL);
    if (sysPart)
    {
        const Bytes sys(image.begin(), image.begin() + kSysSize);
        if (!isOfficialV107(sys) && !isDumpSys(sys))
            return "System area is neither official KORG v1.07 nor verified dump-patched v1.07 (refused for safety)";
    }
    if (t != Type::SYS)
    {
        const size_t hdr = (t == Type::ALL) ? 0x800 : 0;
        if (std::memcmp(image.data() + hdr, "KORG", 4) != 0 || image[hdr + 6] != 0x58)
            return "Missing KORG MS2000 (0x58) header signature";
        if (t != Type::ALL && image[hdr + 7] != static_cast<uint8_t>(t))
            return "Image area-ID byte does not match selected update type";
    }
    return {};
}

// ---- Host -> Unit SysEx Messages ----
static Bytes head(uint8_t cmd)
{
    return {0xF0, 0x42, 0x23, 0x58, cmd};
}

Bytes msgVersion()
{
    Bytes m = head(0x00);
    m.push_back(0xF7);
    return m;
}

Bytes msgSetType(Type t)
{
    Bytes m = head(0x02);
    m.push_back(static_cast<uint8_t>(t));
    m.push_back(0xF7);
    return m;
}

Bytes msgBlock(uint8_t bb, const uint8_t *d)
{
    Bytes m = head(0x04);
    m.reserve(9372);
    m.push_back(bb & 0x7F);
    m.push_back(0x00);
    unsigned sum = 0;
    for (uint32_t g = 0; g < kBlockSize; g += 7)
    {
        const uint32_t n = (kBlockSize - g) < 7 ? (kBlockSize - g) : 7;
        uint8_t msb = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            if (d[g + i] & 0x80)
                msb |= static_cast<uint8_t>(1u << i);
        }
        m.push_back(msb);
        sum += msb;  // MSB byte FIRST (IPL 0x407756, vup_ms2000.exe 0x404A88)
        for (uint32_t i = 0; i < n; ++i)
        {
            const uint8_t low7 = d[g + i] & 0x7F;
            m.push_back(low7);
            sum += low7;
        }
    }
    m.push_back(static_cast<uint8_t>(sum & 0x7F));
    m.push_back(0xF7);
    return m;
}

static void put24(Bytes &m, uint32_t v)
{
    const uint8_t b[3] = {
        static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v)
    };
    for (uint8_t x : b)
        m.push_back(x & 0x7F);
    m.push_back(static_cast<uint8_t>((b[0] & 0x80 ? 1 : 0) | (b[1] & 0x80 ? 2 : 0) | (b[2] & 0x80 ? 4 : 0)));
}

Bytes msgDump(uint32_t addr, uint32_t size)
{
    Bytes m = head(0x01);
    put24(m, addr);
    put24(m, size);
    m.push_back(0xF7);
    return m;
}

// ---- Unit -> Host Replies ----
static bool isIplReply(const Bytes &s, uint8_t cmd, size_t len)
{
    return s.size() == len && s[0] == 0xF0 && s[1] == 0x42 && s[2] == 0x23 && s[3] == 0x58
        && s[4] == cmd && s.back() == 0xF7;
}

bool parseVersion(const Bytes &s, Version &v)
{
    if (!isIplReply(s, 0x01, 12))
        return false;
    v.sys = static_cast<uint16_t>(s[5] | (s[6] << 8));
    v.pcm = static_cast<uint16_t>(s[7] | (s[8] << 8));
    v.usr = static_cast<uint16_t>(s[9] | (s[10] << 8));
    return true;
}

int parseStatus(const Bytes &s)
{
    return isIplReply(s, 0x03, 7) ? static_cast<int>(s[5]) : -1;
}

std::string statusText(int c)
{
    switch (c)
    {
        case 0: return "OK";
        case 1: return "Format Error (fmt): unit rejected message format";
        case 2: return "Checksum Error (sum): MIDI data corrupted in transit";
        case 3: return "Timeout Error";
        case 4: return "Hardware / MIDI Receive Error (hrd): UART overrun or framing error (try a slower Transfer Rate)";
        case 5: return "Flash Write Error (wrt): flash sector erase or word program failed";
    }
    return "Unknown IPL status code " + std::to_string(c);
}

std::string statusLcdCode(int c)
{
    switch (c)
    {
        case 1: return "fmt ";
        case 2: return "sum ";
        case 4: return "hrd ";
        case 5: return "wrt ";
        default: return "err ";
    }
}

std::string versionText(uint16_t v)
{
    if (!v)
        return "-";
    char s[16];
    std::snprintf(s, sizeof(s), "%u.%02u", static_cast<unsigned>(v >> 8), static_cast<unsigned>(v & 0xFFu));
    return s;
}

bool decodeDump(const Bytes &s, uint32_t size, Bytes &out)
{
    out.assign(size, 0);
    size_t j = 1;
    if (s.size() < 2 || s[0] != 0xF0)
        return false;
    uint32_t i = 0, k = 0;
    for (; i < size; ++i)
    {
        if (j >= s.size())
            return false;
        out[i] = s[j++];
        ++k;
        if (k == 7)
        {
            if (j >= s.size())
                return false;
            const uint8_t top = s[j++];
            for (uint32_t n = 0; n < 7; ++n)
                if (top & (1u << n))
                    out[i - 6 + n] |= 0x80;
            k = 0;
        }
    }
    if (k)
    {
        if (j >= s.size())
            return false;
        const uint8_t top = s[j++];
        for (uint32_t n = 0; n < k; ++n)
            if (top & (1u << n))
                out[i - k + n] |= 0x80;
    }
    return j == s.size() - 1 && s[j] == 0xF7;
}

// ---- Hardware LCD 16x2 Simulation Helpers ----
static std::string makeIplLcdLine2(int completedSectors)
{
    std::string row2(16, '.');
    for (int s = 0; s < completedSectors && s < 16; ++s)
        row2[static_cast<size_t>(s)] = '>';
    return row2;
}

static int waitStatus(Transport &t, int timeoutMs)
{
    Bytes s;
    for (int tries = 0; tries < 8; ++tries)
    {
        if (!t.receive(s, timeoutMs))
            return -2;
        const int c = parseStatus(s);
        if (c >= 0)
            return c;
    }
    return -3;
}

Result getVersion(Transport &t, Version &v, int timeoutMs)
{
    Result r;
    r.lcdLine1 = "IPL s.p.u [    ]";
    r.lcdLine2 = "................";

    if (!t.send(msgVersion(), TransferRate::VeryFast))
    {
        r.error = "MIDI send failed — verify MIDI OUT port is selected and available.";
        return r;
    }

    Bytes s;
    for (int tries = 0; tries < 8; ++tries)
    {
        if (!t.receive(s, timeoutMs))
        {
            r.error = "No response from MS2000/R. Verify unit is powered on in IPL mode (hold WRITE + TYPE at power-on, LCD shows \"IPL s.p.u\") and both MIDI IN and MIDI OUT cables are connected.";
            return r;
        }
        if (parseVersion(s, v))
        {
            t.wait(20);
            r.ok = true;
            return r;
        }
    }
    r.error = "Unexpected SysEx reply (not a KORG MS2000 / MS2000R in IPL update mode).";
    return r;
}

Result install(Transport &t,
               Type type,
               const Bytes &image,
               TransferRate rate,
               const ProgressCallback &progress)
{
    Result r;
    r.lcdLine1 = "IPL s.p.u [    ]";
    r.lcdLine2 = "................";

    if (const std::string e = validateImage(type, image); !e.empty())
    {
        r.error = e;
        return r;
    }

    const int n = static_cast<int>(image.size() / kBlockSize);
    const std::string again = " The unit is still running in IPL mode in RAM: DO NOT power it off — click Update again to retry cleanly from Block 0.";

    if (progress)
    {
        ProgressUpdate u {0, n, "Scanning flash sectors (Set Update Type)...", "IPL s.p.u [SYS ]", "................"};
        if (!progress(u))
        {
            r.error = "Cancelled before starting transfer.";
            return r;
        }
    }

    if (!t.send(msgSetType(type), TransferRate::VeryFast))
    {
        r.error = "MIDI send failed on Set-Type.";
        return r;
    }

    int c = waitStatus(t, 20000);
    if (c != 0)
    {
        r.statusCode = c;
        r.lcdLine1 = (c > 0) ? ("IPL s.p.u [" + statusLcdCode(c) + "]") : "IPL s.p.u [    ]";
        r.error = (c < 0) ? "Timeout waiting for Set-Type acknowledgement." : ("Set-Type failed: " + statusText(c));
        return r;
    }
    t.wait(20);

    for (int bb = 0; bb < n; ++bb)
    {
        const int completedSectors = bb / 8;
        const std::string lcd1 = "IPL s.p.u [SYS ]";
        const std::string lcd2 = makeIplLcdLine2(completedSectors);

        if (progress)
        {
            char stageBuf[96];
            std::snprintf(stageBuf,
                          sizeof(stageBuf),
                          "Writing Block %d / %d (Flash 0x%05X..0x%05X)",
                          bb + 1,
                          n,
                          static_cast<unsigned>(bb * kBlockSize),
                          static_cast<unsigned>((bb + 1) * kBlockSize - 1));
            ProgressUpdate u {bb, n, stageBuf, lcd1, lcd2};
            if (!progress(u))
            {
                r.lcdLine1 = lcd1;
                r.lcdLine2 = lcd2;
                r.error = "Stopped by user after block " + std::to_string(bb) + "." + again;
                return r;
            }
        }

        if (!t.send(msgBlock(static_cast<uint8_t>(bb), image.data() + static_cast<size_t>(bb) * kBlockSize), rate))
        {
            r.lcdLine1 = lcd1;
            r.lcdLine2 = lcd2;
            r.error = "MIDI send failed at block " + std::to_string(bb) + "." + again;
            return r;
        }

        c = waitStatus(t, 30000);
        if (c != 0)
        {
            r.statusCode = c;
            r.lcdLine1 = (c > 0) ? ("IPL s.p.u [" + statusLcdCode(c) + "]") : lcd1;
            r.lcdLine2 = lcd2;
            r.error = "Block " + std::to_string(bb) + ": "
                    + (c < 0 ? std::string("Receive Error (Timeout)") : statusText(c)) + "." + again;
            return r;
        }
        t.wait(20);
    }

    r.ok = true;
    r.lcdLine1 = "IPL S.p.u [SYS ]";
    r.lcdLine2 = makeIplLcdLine2(n / 8);
    if (progress)
    {
        ProgressUpdate u {n, n, "Update Complete! Power cycle the MS2000/R.", r.lcdLine1, r.lcdLine2};
        progress(u);
    }
    return r;
}

Result dump(Transport &t,
            uint32_t addr,
            uint32_t size,
            Bytes &out,
            const ProgressCallback &progress,
            int retries)
{
    Result r;
    constexpr uint32_t kChunk = 0x80;  // 128 bytes per dump packet
    out.clear();
    out.reserve(size);

    const int total = static_cast<int>((size + kChunk - 1) / kChunk);
    for (uint32_t off = 0; off < size; off += kChunk)
    {
        const uint32_t len = (size - off) < kChunk ? (size - off) : kChunk;
        const int step = static_cast<int>(off / kChunk);

        if (progress && (step % 16 == 0 || off + len >= size))
        {
            char stageBuf[96];
            std::snprintf(stageBuf,
                          sizeof(stageBuf),
                          "Reading Flash 0x%06X / 0x%06X (%u KB / %u KB)",
                          static_cast<unsigned>(addr + off),
                          static_cast<unsigned>(addr + size),
                          static_cast<unsigned>(off / 1024),
                          static_cast<unsigned>(size / 1024));
            ProgressUpdate u {step, total, stageBuf, "IPL s.p.u [    ]", "DUMPING........."};
            if (!progress(u))
            {
                r.error = "Flash backup cancelled by user.";
                r.lcdLine1 = "IPL s.p.u [    ]";
                r.lcdLine2 = "................";
                return r;
            }
        }

        bool ok = false;
        for (int a = 0; a <= retries && !ok; ++a)
        {
            if (a > 0)
                t.wait(300);
            if (!t.send(msgDump(addr + off, len), TransferRate::VeryFast))
                continue;
            Bytes s, d;
            if (t.receive(s, 3000) && decodeDump(s, len, d))
            {
                out.insert(out.end(), d.begin(), d.end());
                ok = true;
            }
            t.wait(20);
        }

        if (!ok)
        {
            char e[128];
            std::snprintf(e,
                          sizeof(e),
                          "No valid dump reply at address 0x%06X after %d attempts.",
                          static_cast<unsigned>(addr + off),
                          retries + 1);
            r.error = std::string(e)
                    + " Make sure the Flash-Dump IPL Patch is installed and the unit was power-cycled back into IPL mode (WRITE + TYPE).";
            r.lcdLine1 = "IPL s.p.u [    ]";
            r.lcdLine2 = "................";
            return r;
        }
    }

    r.ok = true;
    r.lcdLine1 = "IPL s.p.u [    ]";
    r.lcdLine2 = "................";
    if (progress)
    {
        ProgressUpdate u {total, total, "Flash read complete!", r.lcdLine1, r.lcdLine2};
        progress(u);
    }
    return r;
}

}  // namespace ms2kipl

// ─────────────────────────────────────────────────────────────────────────────
// Adapter from HardwareMidiTransport to ms2kipl::Transport
// ─────────────────────────────────────────────────────────────────────────────
namespace
{

class HardwareIplTransportAdapter final : public ms2kipl::Transport
{
  public:
    explicit HardwareIplTransportAdapter(HardwareMidiTransport &hw) : hw_(hw) {}

    bool prepare(std::string &err)
    {
        if (!hw_.hasSelectedOutput() || !hw_.hasSelectedInput())
        {
            err = "Please select both MIDI IN and MIDI OUT ports first.";
            return false;
        }
        if (!hw_.listen())
        {
            err = hw_.getLastDetail().toStdString();
            if (err.empty())
                err = "Could not open the selected MIDI IN port.";
            return false;
        }
        hw_.flushSysExQueue();
        return true;
    }

    bool send(const ms2kipl::Bytes &msg, ms2kipl::TransferRate rate) override
    {
        hw_.flushSysExQueue();

        // Korg vup_ms2000.exe sends the full SysEx frame; for slower MIDI interfaces,
        // we also add pre/post pacing according to TransferRate.
        const juce::MemoryBlock block(msg.data(), msg.size());
        if (!hw_.send(block))
            return false;

        switch (rate)
        {
            case ms2kipl::TransferRate::VeryFast:
                break;
            case ms2kipl::TransferRate::Fast:
                wait(80);
                break;
            case ms2kipl::TransferRate::Slow:
                wait(220);
                break;
            case ms2kipl::TransferRate::VerySlow:
                wait(450);
                break;
        }
        return true;
    }

    bool receive(ms2kipl::Bytes &sysex, int timeoutMs) override
    {
        return hw_.waitForSysEx(sysex, timeoutMs);
    }

    void wait(int ms) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

  private:
    HardwareMidiTransport &hw_;
};

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// MS2000IplUpdater implementation
// ─────────────────────────────────────────────────────────────────────────────

MS2000IplUpdater::MS2000IplUpdater()
{
    if (DWGSBinary::x811v107_sys != nullptr && DWGSBinary::x811v107_sysSize == ms2kipl::kSysSize)
    {
        const auto *raw = reinterpret_cast<const uint8_t *>(DWGSBinary::x811v107_sys);
        embeddedV107_.assign(raw, raw + DWGSBinary::x811v107_sysSize);
        embeddedSha1_ = ms2kipl::sha1Hex(embeddedV107_);
        std::string err;
        if (embeddedSha1_ == ms2kipl::kV107Sha1 && ms2kipl::makeDumpSys(embeddedV107_, embeddedDumpSys_, err))
        {
            dumpSysSha1_   = ms2kipl::sha1Hex(embeddedDumpSys_);
            embeddedValid_ = (dumpSysSha1_ == ms2kipl::kDumpSysSha1);
        }
    }
}

MS2000IplUpdater::~MS2000IplUpdater()
{
    cancel_.store(true);
    joinWorker();
}

void MS2000IplUpdater::joinWorker()
{
    std::lock_guard<std::mutex> lock(workerMutex_);
    if (worker_.joinable())
        worker_.join();
}

void MS2000IplUpdater::startCheckVersion(HardwareMidiTransport &transport, bool silentPoll, EventCallback emit)
{
    if (busy_.load())
        return;

    joinWorker();
    cancel_.store(false);
    busy_.store(true);

    std::lock_guard<std::mutex> lock(workerMutex_);
    worker_ = std::thread(
        [this, &transport, silentPoll, emit = std::move(emit)]()
        {
            HardwareIplTransportAdapter adapter(transport);
            std::string prepErr;
            if (!adapter.prepare(prepErr))
            {
                busy_.store(false);
                if (!silentPoll && emit)
                {
                    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                    obj->setProperty("ok", false);
                    obj->setProperty("silent", silentPoll);
                    obj->setProperty("error", juce::String(prepErr));
                    emit("ipl.version", juce::JSON::toString(juce::var(obj.get()), true).toStdString());
                }
                return;
            }

            ms2kipl::Version ver;
            const auto res = ms2kipl::getVersion(adapter, ver, silentPoll ? 900 : 2500);
            busy_.store(false);

            if (emit)
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("ok", res.ok);
                obj->setProperty("silent", silentPoll);
                if (res.ok)
                {
                    obj->setProperty("sysRaw", static_cast<int>(ver.sys));
                    obj->setProperty("pcmRaw", static_cast<int>(ver.pcm));
                    obj->setProperty("usrRaw", static_cast<int>(ver.usr));
                    obj->setProperty("sysVersion", juce::String(ms2kipl::versionText(ver.sys)));
                    obj->setProperty("pcmVersion", juce::String(ms2kipl::versionText(ver.pcm)));
                    obj->setProperty("usrVersion", juce::String(ms2kipl::versionText(ver.usr)));
                    obj->setProperty("isV107", ver.sys == 0x0107);
                    obj->setProperty("lcdLine1", juce::String(res.lcdLine1));
                    obj->setProperty("lcdLine2", juce::String(res.lcdLine2));
                }
                else
                {
                    obj->setProperty("error", juce::String(res.error));
                }
                emit("ipl.version", juce::JSON::toString(juce::var(obj.get()), true).toStdString());
            }
        });
}

void MS2000IplUpdater::startInstallSystem(HardwareMidiTransport &transport,
                                          bool installDumpPatch,
                                          ms2kipl::TransferRate rate,
                                          EventCallback emit)
{
    if (busy_.load())
        return;

    joinWorker();
    cancel_.store(false);
    busy_.store(true);

    std::lock_guard<std::mutex> lock(workerMutex_);
    worker_ = std::thread(
        [this, &transport, installDumpPatch, rate, emit = std::move(emit)]()
        {
            if (!embeddedValid_)
            {
                busy_.store(false);
                if (emit)
                {
                    juce::DynamicObject::Ptr errObj = new juce::DynamicObject();
                    errObj->setProperty("ok", false);
                    errObj->setProperty("error", "Embedded x811v107.sys firmware failed SHA-1 integrity verification.");
                    emit("ipl.completed", juce::JSON::toString(juce::var(errObj.get()), true).toStdString());
                }
                return;
            }

            HardwareIplTransportAdapter adapter(transport);
            std::string prepErr;
            if (!adapter.prepare(prepErr))
            {
                busy_.store(false);
                if (emit)
                {
                    juce::DynamicObject::Ptr errObj = new juce::DynamicObject();
                    errObj->setProperty("ok", false);
                    errObj->setProperty("error", juce::String(prepErr));
                    emit("ipl.completed", juce::JSON::toString(juce::var(errObj.get()), true).toStdString());
                }
                return;
            }

            // First verify unit is alive in IPL mode
            ms2kipl::Version ver;
            const auto verRes = ms2kipl::getVersion(adapter, ver, 2500);
            if (!verRes.ok)
            {
                busy_.store(false);
                if (emit)
                {
                    juce::DynamicObject::Ptr errObj = new juce::DynamicObject();
                    errObj->setProperty("ok", false);
                    errObj->setProperty("error", juce::String(verRes.error));
                    emit("ipl.completed", juce::JSON::toString(juce::var(errObj.get()), true).toStdString());
                }
                return;
            }

            const auto &image = installDumpPatch ? embeddedDumpSys_ : embeddedV107_;
            const auto t0 = std::chrono::steady_clock::now();

            const auto res = ms2kipl::install(
                adapter,
                ms2kipl::Type::SYS,
                image,
                rate,
                [this, &emit](const ms2kipl::ProgressUpdate &u) -> bool
                {
                    if (emit)
                    {
                        juce::DynamicObject::Ptr p = new juce::DynamicObject();
                        p->setProperty("operation", "install");
                        p->setProperty("done", u.done);
                        p->setProperty("total", u.total);
                        p->setProperty("percent", u.total > 0 ? (100.0 * u.done / u.total) : 0.0);
                        p->setProperty("stage", juce::String(u.stage));
                        p->setProperty("lcdLine1", juce::String(u.lcdLine1));
                        p->setProperty("lcdLine2", juce::String(u.lcdLine2));
                        emit("ipl.progress", juce::JSON::toString(juce::var(p.get()), true).toStdString());
                    }
                    return !cancel_.load();
                });

            const double elapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            busy_.store(false);

            if (emit)
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("ok", res.ok);
                obj->setProperty("operation", installDumpPatch ? "installDumpPatch" : "installOfficialV107");
                obj->setProperty("elapsedSeconds", elapsedSec);
                obj->setProperty("lcdLine1", juce::String(res.lcdLine1));
                obj->setProperty("lcdLine2", juce::String(res.lcdLine2));
                if (!res.ok)
                    obj->setProperty("error", juce::String(res.error));
                emit("ipl.completed", juce::JSON::toString(juce::var(obj.get()), true).toStdString());
            }
        });
}

void MS2000IplUpdater::startFlashDump(HardwareMidiTransport &transport, uint32_t sizeBytes, EventCallback emit)
{
    if (busy_.load())
        return;

    joinWorker();
    cancel_.store(false);
    busy_.store(true);

    std::lock_guard<std::mutex> lock(workerMutex_);
    worker_ = std::thread(
        [this, &transport, sizeBytes, emit = std::move(emit)]()
        {
            HardwareIplTransportAdapter adapter(transport);
            std::string prepErr;
            if (!adapter.prepare(prepErr))
            {
                busy_.store(false);
                if (emit)
                {
                    juce::DynamicObject::Ptr errObj = new juce::DynamicObject();
                    errObj->setProperty("ok", false);
                    errObj->setProperty("error", juce::String(prepErr));
                    emit("ipl.dumpCompleted", juce::JSON::toString(juce::var(errObj.get()), true).toStdString());
                }
                return;
            }

            ms2kipl::Bytes dumpData;
            const uint32_t clampedSize = (sizeBytes == ms2kipl::kSysSize) ? ms2kipl::kSysSize : ms2kipl::kFlashSize;
            const auto t0 = std::chrono::steady_clock::now();

            const auto res = ms2kipl::dump(
                adapter,
                0,
                clampedSize,
                dumpData,
                [this, &emit](const ms2kipl::ProgressUpdate &u) -> bool
                {
                    if (emit)
                    {
                        juce::DynamicObject::Ptr p = new juce::DynamicObject();
                        p->setProperty("operation", "dump");
                        p->setProperty("done", u.done);
                        p->setProperty("total", u.total);
                        p->setProperty("percent", u.total > 0 ? (100.0 * u.done / u.total) : 0.0);
                        p->setProperty("stage", juce::String(u.stage));
                        p->setProperty("lcdLine1", juce::String(u.lcdLine1));
                        p->setProperty("lcdLine2", juce::String(u.lcdLine2));
                        emit("ipl.progress", juce::JSON::toString(juce::var(p.get()), true).toStdString());
                    }
                    return !cancel_.load();
                });

            const double elapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            busy_.store(false);

            if (emit)
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("ok", res.ok);
                obj->setProperty("elapsedSeconds", elapsedSec);
                obj->setProperty("lcdLine1", juce::String(res.lcdLine1));
                obj->setProperty("lcdLine2", juce::String(res.lcdLine2));

                if (res.ok && !dumpData.empty())
                {
                    const std::string fullSha1 = ms2kipl::sha1Hex(dumpData);
                    bool sysVerified = false;
                    if (dumpData.size() >= ms2kipl::kSysSize)
                    {
                        const ms2kipl::Bytes sysArea(dumpData.begin(), dumpData.begin() + ms2kipl::kSysSize);
                        sysVerified = ms2kipl::isDumpSys(sysArea) || ms2kipl::isOfficialV107(sysArea);
                    }

                    obj->setProperty("size", static_cast<int>(dumpData.size()));
                    obj->setProperty("sha1", juce::String(fullSha1));
                    obj->setProperty("sysVerified", sysVerified);
                    obj->setProperty("dataBase64", juce::Base64::toBase64(dumpData.data(), dumpData.size()));
                }
                else
                {
                    obj->setProperty("error", juce::String(res.error));
                }
                emit("ipl.dumpCompleted", juce::JSON::toString(juce::var(obj.get()), true).toStdString());
            }
        });
}

}  // namespace ABDMS2000
