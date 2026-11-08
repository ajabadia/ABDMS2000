#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace ABDMS2000
{

/**
 * @brief Tablas de conversión numérica exactas extraídas del firmware oficial
 *        Korg MS2000 v1.07 (`x811v107.sys`, Hitachi H8S/2350 -> Motorola DSP56362).
 *
 * En el hardware real:
 *  - El cristal maestro es de 12.288 MHz (256 * fs) dividido por un 74HC4040 para
 *    generar EXTAL = 3.072 MHz (64 * fs) y FS1/FS2/FS3 = 48.000 kHz exactos.
 *  - El DSP56362 multiplica EXTAL mediante su PLL (`PCTL = $AD0165`, 358 / 11)
 *    hasta 99.98 MHz y opera en coma fija fraccional de 24 bits con signo (`Q23`,
 *    donde `0x7FFFFF` = +1.0 = 8,388,607).
 *  - El H8S traduce los valores MIDI/SysEx/Knobs (0..127) a palabras de 24 bits
 *    consultando estas LUTs en ROM (`0x240EC..0x2EE00`).
 */
namespace FirmwareTables
{

inline constexpr double kHardwareSampleRate = 48000.0;
inline constexpr uint32_t kQ23One = 0x7FFFFFu;  // 2^23 - 1 = 8,388,607

// ============================================================================
// 1. Tabla de Incremento de Envolventes EG1 / EG2 (ROM 0x27D54, 128 entradas)
//    Cada entrada es el incremento lineal por muestra en Q23 a 48.000 kHz:
//      N_samples(knob) = 0x7FFFFF / kRomEnvelopeIncrementQ23[knob]
//      Tiempo_segundos(knob) = N_samples(knob) / 48000.0
//    En knob=0: 0x02AAAA (174,762) -> 48 muestras = 1.000 ms exactos.
//    En knob=127: 0x00002E (46) -> 182,361 muestras = 3.7992 s.
// ============================================================================
inline constexpr uint32_t kRomEnvelopeIncrementQ23[128] = {
    0x02AAAAu, 0x012CB2u, 0x010410u, 0x00EB0Eu, 0x00D20Du, 0x00C11Cu, 0x00B02Cu, 0x00A3F0u, 0x0097B4u, 0x008534u,
    0x0076B9u, 0x006B15u, 0x006186u, 0x005987u, 0x0052BFu, 0x004CEBu, 0x0047DCu, 0x00436Cu, 0x003F80u, 0x003C03u,
    0x0038E3u, 0x003612u, 0x0031A6u, 0x002DE4u, 0x002AAAu, 0x0027DDu, 0x002568u, 0x00233Cu, 0x00214Du, 0x001F91u,
    0x001E01u, 0x001C97u, 0x001B4Eu, 0x001A01u, 0x0018D3u, 0x0017BEu, 0x0016C1u, 0x0015D8u, 0x001501u, 0x00143Au,
    0x001381u, 0x0012D5u, 0x001181u, 0x001059u, 0x000F57u, 0x000E72u, 0x000DA7u, 0x000CF1u, 0x000C4Cu, 0x000BB8u,
    0x000B30u, 0x000AB5u, 0x000A44u, 0x0009DBu, 0x00097Bu, 0x000921u, 0x0008CFu, 0x000881u, 0x000839u, 0x0007F6u,
    0x0007B6u, 0x00077Bu, 0x000743u, 0x00070Eu, 0x0006DCu, 0x0006ADu, 0x000680u, 0x000655u, 0x00062Du, 0x000607u,
    0x0005E2u, 0x0005BFu, 0x00059Eu, 0x00057Eu, 0x000560u, 0x000542u, 0x000526u, 0x00050Cu, 0x0004F2u, 0x0004D9u,
    0x0004C1u, 0x0004AAu, 0x000494u, 0x00047Fu, 0x00046Bu, 0x000457u, 0x000444u, 0x000431u, 0x00041Fu, 0x00040Eu,
    0x0003FEu, 0x0003EDu, 0x0003DEu, 0x0003CEu, 0x0003C0u, 0x0003B1u, 0x0003A4u, 0x000396u, 0x000389u, 0x00037Cu,
    0x000370u, 0x000364u, 0x000358u, 0x000308u, 0x0002BBu, 0x00025Fu, 0x000219u, 0x0001E2u, 0x0001B4u, 0x00018Fu,
    0x00016Fu, 0x00013Eu, 0x00011Du, 0x000102u, 0x0000ECu, 0x0000D9u, 0x0000C9u, 0x0000B3u, 0x0000A1u, 0x000092u,
    0x000086u, 0x00007Cu, 0x00006Fu, 0x000057u, 0x000047u, 0x00003Cu, 0x000034u, 0x00002Eu,
};

// ============================================================================
// 2. Tabla de Incremento de LFO1 (ROM 0x295E8, 128 entradas, 24-bit Q23)
// ============================================================================
inline constexpr uint32_t kRomLfo1IncrementQ23[128] = {
    0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u,
    0x000010u, 0x000010u, 0x000010u, 0x000012u, 0x000012u, 0x000012u, 0x000012u, 0x000014u, 0x000014u, 0x000016u,
    0x000018u, 0x00001Au, 0x00001Cu, 0x00001Eu, 0x000020u, 0x000022u, 0x000026u, 0x00002Au, 0x00002Eu, 0x000032u,
    0x000038u, 0x00003Eu, 0x000044u, 0x00004Au, 0x000052u, 0x00005Au, 0x000062u, 0x00006Cu, 0x000076u, 0x000082u,
    0x00008Eu, 0x00009Au, 0x0000A8u, 0x0000B8u, 0x0000C8u, 0x0000DAu, 0x0000ECu, 0x000100u, 0x000114u, 0x00012Au,
    0x000142u, 0x00015Cu, 0x000176u, 0x000194u, 0x0001B2u, 0x0001D2u, 0x0001F2u, 0x000216u, 0x00023Cu, 0x000264u,
    0x00028Cu, 0x0002B8u, 0x0002E6u, 0x000316u, 0x000348u, 0x00037Cu, 0x0003B4u, 0x0003EEu, 0x00042Au, 0x00046Au,
    0x0004ACu, 0x0004F0u, 0x000538u, 0x000582u, 0x0005D0u, 0x000622u, 0x000676u, 0x0006CEu, 0x00072Au, 0x00078Au,
    0x0007ECu, 0x000852u, 0x0008BCu, 0x00092Cu, 0x00099Eu, 0x000A14u, 0x000A90u, 0x000B0Eu, 0x000B92u, 0x000C1Au,
    0x000CA8u, 0x000D38u, 0x000DD0u, 0x000E6Au, 0x000F0Cu, 0x000FB2u, 0x00105Cu, 0x00110Eu, 0x0011C4u, 0x001280u,
    0x001340u, 0x001408u, 0x0014D6u, 0x0015AAu, 0x001684u, 0x001764u, 0x00184Au, 0x001938u, 0x001A2Cu, 0x001B26u,
    0x001C28u, 0x001D32u, 0x001E42u, 0x001F5Au, 0x00207Au, 0x0021A0u, 0x0022D0u, 0x002406u, 0x002544u, 0x00268Cu,
    0x0027DCu, 0x002932u, 0x002A94u, 0x002BFCu, 0x002D6Eu, 0x002EEAu, 0x00306Eu, 0x0031FCu,
};

// ============================================================================
// 3. Tabla de Incremento de LFO2 (ROM 0x29BE8, 128 entradas, 24-bit Q23)
//    Idéntica a LFO1 hasta el índice 114 (última palabra común: 0x00207A);
//    difiere por primera vez en 115 y llega a 194.904 Hz en 127 (5.3239x LFO1).
//    El límite general de 500 Hz del LFO permite modulación adicional;
//    no es un valor producido por esta tabla.
// ============================================================================
inline constexpr uint32_t kRomLfo2IncrementQ23[128] = {
    0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u, 0x000010u,
    0x000010u, 0x000010u, 0x000010u, 0x000012u, 0x000012u, 0x000012u, 0x000012u, 0x000014u, 0x000014u, 0x000016u,
    0x000018u, 0x00001Au, 0x00001Cu, 0x00001Eu, 0x000020u, 0x000022u, 0x000026u, 0x00002Au, 0x00002Eu, 0x000032u,
    0x000038u, 0x00003Eu, 0x000044u, 0x00004Au, 0x000052u, 0x00005Au, 0x000062u, 0x00006Cu, 0x000076u, 0x000082u,
    0x00008Eu, 0x00009Au, 0x0000A8u, 0x0000B8u, 0x0000C8u, 0x0000DAu, 0x0000ECu, 0x000100u, 0x000114u, 0x00012Au,
    0x000142u, 0x00015Cu, 0x000176u, 0x000194u, 0x0001B2u, 0x0001D2u, 0x0001F2u, 0x000216u, 0x00023Cu, 0x000264u,
    0x00028Cu, 0x0002B8u, 0x0002E6u, 0x000316u, 0x000348u, 0x00037Cu, 0x0003B4u, 0x0003EEu, 0x00042Au, 0x00046Au,
    0x0004ACu, 0x0004F0u, 0x000538u, 0x000582u, 0x0005D0u, 0x000622u, 0x000676u, 0x0006CEu, 0x00072Au, 0x00078Au,
    0x0007ECu, 0x000852u, 0x0008BCu, 0x00092Cu, 0x00099Eu, 0x000A14u, 0x000A90u, 0x000B0Eu, 0x000B92u, 0x000C1Au,
    0x000CA8u, 0x000D38u, 0x000DD0u, 0x000E6Au, 0x000F0Cu, 0x000FB2u, 0x00105Cu, 0x00110Eu, 0x0011C4u, 0x001280u,
    0x001340u, 0x001408u, 0x0014D6u, 0x0015AAu, 0x001684u, 0x001764u, 0x00184Au, 0x001938u, 0x001A2Cu, 0x001B26u,
    0x001C28u, 0x001D32u, 0x001E42u, 0x001F5Au, 0x00207Au, 0x0020D4u, 0x0020D6u, 0x0020EEu, 0x002152u, 0x002272u,
    0x002514u, 0x002A5Cu, 0x0033F2u, 0x004406u, 0x005D76u, 0x0083DCu, 0x00BBA2u, 0x010A1Cu,
};

// ============================================================================
// 4. Tabla Maestra de Pitch / Pre-warping Bilineal (ROM 0x240F0, 256 entradas)
// ============================================================================
inline constexpr uint32_t kRomMasterPitchQ23[256] = {
    0x00000Bu, 0x00000Bu, 0x00000Cu, 0x00000Du, 0x00000Du, 0x00000Eu, 0x00000Fu, 0x000010u, 0x000011u, 0x000012u,
    0x000013u, 0x000014u, 0x000016u, 0x000017u, 0x000018u, 0x00001Au, 0x00001Bu, 0x00001Du, 0x00001Fu, 0x000021u,
    0x000023u, 0x000025u, 0x000027u, 0x000029u, 0x00002Cu, 0x00002Eu, 0x000031u, 0x000034u, 0x000037u, 0x00003Au,
    0x00003Eu, 0x000042u, 0x000046u, 0x00004Au, 0x00004Eu, 0x000053u, 0x000058u, 0x00005Du, 0x000063u, 0x000069u,
    0x00006Fu, 0x000075u, 0x00007Cu, 0x000084u, 0x00008Cu, 0x000094u, 0x00009Du, 0x0000A6u, 0x0000B0u, 0x0000BBu,
    0x0000C6u, 0x0000D2u, 0x0000DEu, 0x0000EBu, 0x0000F9u, 0x000108u, 0x000118u, 0x000129u, 0x00013Au, 0x00014Du,
    0x000161u, 0x000176u, 0x00018Cu, 0x0001A4u, 0x0001BDu, 0x0001D7u, 0x0001F3u, 0x000211u, 0x000231u, 0x000252u,
    0x000275u, 0x00029Bu, 0x0002C2u, 0x0002ECu, 0x000319u, 0x000348u, 0x00037Au, 0x0003AFu, 0x0003E7u, 0x000423u,
    0x000461u, 0x0004A4u, 0x0004EBu, 0x000536u, 0x000585u, 0x0005D9u, 0x000632u, 0x000690u, 0x0006F4u, 0x00075Eu,
    0x0007CEu, 0x000845u, 0x0008C3u, 0x000948u, 0x0009D6u, 0x000A6Bu, 0x000B0Au, 0x000BB2u, 0x000C64u, 0x000D20u,
    0x000DE8u, 0x000EBBu, 0x000F9Cu, 0x001089u, 0x001185u, 0x00128Fu, 0x0013A9u, 0x0014D4u, 0x001611u, 0x001761u,
    0x0018C4u, 0x001A3Du, 0x001BCCu, 0x001D72u, 0x001F32u, 0x00210Cu, 0x002305u, 0x002519u, 0x002750u, 0x0029A5u,
    0x002C24u, 0x002ECDu, 0x003198u, 0x00348Du, 0x0037AAu, 0x003AFCu, 0x003E7Bu, 0x00422Fu, 0x004621u, 0x004A4Eu,
    0x004EB5u, 0x005365u, 0x005862u, 0x005D9Eu, 0x00632Fu, 0x00691Eu, 0x006F55u, 0x0075FCu, 0x007CF6u, 0x00846Cu,
    0x008C49u, 0x0094A8u, 0x009D7Au, 0x00A6D1u, 0x00B0C2u, 0x00BB49u, 0x00C67Eu, 0x00D24Du, 0x00DECFu, 0x00EBFBu,
    0x00F9FFu, 0x0108D6u, 0x011890u, 0x012941u, 0x013ADCu, 0x014DAEu, 0x01614Eu, 0x01767Fu, 0x018CA9u, 0x01A449u,
    0x01BD36u, 0x01D7CDu, 0x01F3CDu, 0x02116Du, 0x0230EDu, 0x025240u, 0x02759Du, 0x029B19u, 0x02C2BFu, 0x02ECC4u,
    0x031940u, 0x034854u, 0x037A4Fu, 0x03AF41u, 0x03E71Cu, 0x042260u, 0x046185u, 0x04A3E9u, 0x04EA42u, 0x053541u,
    0x058412u, 0x05D7CDu, 0x063063u, 0x068E62u, 0x06F198u, 0x075B09u, 0x07CA69u, 0x08408Fu, 0x08BD2Cu, 0x0941D9u,
    0x09CD4Fu, 0x0A6107u, 0x0AFD79u, 0x0BA31Fu, 0x0C527Du, 0x0D0B23u, 0x0DCE7Au, 0x0E9CA5u, 0x0F7787u, 0x105E74u,
    0x115280u, 0x11F271u, 0x12D5EFu, 0x13C2A8u, 0x14B8CEu, 0x15B892u, 0x16C21Fu, 0x17D59Eu, 0x18F334u, 0x1A1AFFu,
    0x1B4D1Au, 0x1C8999u, 0x1DA492u, 0x1EC661u, 0x1FEEDCu, 0x211DD1u, 0x225301u, 0x238E25u, 0x24CEE7u, 0x2614E3u,
    0x275FA4u, 0x28AEA5u, 0x2A014Au, 0x2B56E1u, 0x2CAE9Bu, 0x2E078Du, 0x2F60A6u, 0x30B8B1u, 0x320E47u, 0x335FCFu,
    0x34AB73u, 0x35EF18u, 0x372854u, 0x385462u, 0x397014u, 0x3A77C5u, 0x3B6746u, 0x3C39C7u, 0x3CE9C4u, 0x3DC28Eu,
    0x3EB851u, 0x3FAE13u, 0x40A3D6u, 0x419999u, 0x428F5Bu, 0x43851Eu, 0x447AE0u, 0x4570A3u, 0x466665u, 0x475C28u,
    0x4851EAu, 0x4947ADu, 0x4A3D70u, 0x4B3332u, 0x4C28F5u, 0x4D1EB7u,
};

// ============================================================================
// 5. Tabla de Frecuencia de Corte VCF (ROM 0x26154, 256 entradas)
// ============================================================================
inline constexpr uint32_t kRomFilterCutoffQ23[256] = {
    0x000038u, 0x000070u, 0x0000A8u, 0x0000E0u, 0x000118u, 0x000150u, 0x000187u, 0x0001BFu, 0x0001F7u, 0x00022Fu,
    0x000267u, 0x00029Fu, 0x0002D7u, 0x00030Fu, 0x000347u, 0x00037Fu, 0x0003B7u, 0x0003EFu, 0x000427u, 0x00045Eu,
    0x000496u, 0x0004CEu, 0x000506u, 0x00053Eu, 0x000576u, 0x00065Fu, 0x000748u, 0x00091Au, 0x000AECu, 0x000CBEu,
    0x000E90u, 0x001062u, 0x001234u, 0x001406u, 0x0015D8u, 0x001893u, 0x001B4Fu, 0x001E0Au, 0x0020C5u, 0x002380u,
    0x00263Bu, 0x0028F6u, 0x002BB1u, 0x002E6Cu, 0x003127u, 0x0033E2u, 0x00369Du, 0x003958u, 0x003C13u, 0x003ECEu,
    0x004189u, 0x004444u, 0x0046FFu, 0x0049BAu, 0x004C75u, 0x004F30u, 0x0051ECu, 0x0054A7u, 0x005762u, 0x005B06u,
    0x005EAAu, 0x00624Eu, 0x0065F2u, 0x006996u, 0x006D3Au, 0x00756Bu, 0x007D9Cu, 0x0085CDu, 0x008DFFu, 0x009630u,
    0x009F1Eu, 0x00A894u, 0x00B29Au, 0x00BD39u, 0x00C87Au, 0x00D465u, 0x00E107u, 0x00EE68u, 0x00FC95u, 0x010B9Au,
    0x011B84u, 0x012C60u, 0x013E3Cu, 0x015128u, 0x016535u, 0x017A72u, 0x0190F3u, 0x01A8CBu, 0x01C20Du, 0x01DCD0u,
    0x01F92Au, 0x021734u, 0x023708u, 0x0258BFu, 0x027C78u, 0x02A251u, 0x02CA6Au, 0x02F4E5u, 0x0321E7u, 0x035196u,
    0x03841Au, 0x03B9A0u, 0x03F255u, 0x042E69u, 0x046E0Fu, 0x04B17Eu, 0x04F8F0u, 0x0544A1u, 0x0594D3u, 0x05E9C9u,
    0x0643CDu, 0x06A32Bu, 0x070835u, 0x077340u, 0x07E4AAu, 0x085CD1u, 0x08DC1Eu, 0x0962FDu, 0x09F1E0u, 0x0A8943u,
    0x0B29A6u, 0x0BD393u, 0x0C879Au, 0x0D4656u, 0x0E1069u, 0x0EE681u, 0x0FC953u, 0x10B9A3u, 0x11B83Cu, 0x12C5F9u,
    0x13E3C0u, 0x151286u, 0x16534Cu, 0x17A726u, 0x190F34u, 0x1A8CACu, 0x1C20D3u, 0x1DCD02u, 0x1F92A7u, 0x217345u,
    0x237078u, 0x258BF2u, 0x27C781u, 0x2A250Cu, 0x2CA698u, 0x2F4E4Bu, 0x321E69u, 0x351958u, 0x3841A6u, 0x3B9A04u,
    0x3F254Eu, 0x42E68Bu, 0x46E0F0u, 0x4B17E5u, 0x4F8F01u, 0x544A17u, 0x594D31u, 0x5E9C96u, 0x643CD2u, 0x6A32B1u,
    0x70834Cu, 0x773407u, 0x7E4A9Bu, 0x85CD15u, 0x8DC1E1u, 0x962FC9u, 0x9F1E03u, 0xA8942Eu, 0xB29A62u, 0xBD392Du,
    0xC879A3u, 0xD46562u, 0xE10697u, 0xEE680Fu, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
    0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u, 0xFC9536u,
};

/**
 * @brief Devuelve el tiempo de envolvente en segundos según la tabla ROM 0x27D54
 *        a la frecuencia de muestreo hardware de 48.000 kHz.
 */
inline float getHardwareEnvelopeTimeSeconds(int midiValue0to127) noexcept
{
    const int idx = std::clamp(midiValue0to127, 0, 127);
    const uint32_t incQ23 = kRomEnvelopeIncrementQ23[idx];
    if (incQ23 == 0)
        return 10.0f;
    const double samples = static_cast<double>(kQ23One) / static_cast<double>(incQ23);
    return static_cast<float>(samples / kHardwareSampleRate);
}

/**
 * @brief Factor de aceleración de LFO2 respecto a LFO1 extraído de las tablas ROM
 *        0x295E8 (LFO1) y 0x29BE8 (LFO2).
 *
 * Para valores de knob 0..114 ambos LFOs tienen exactamente el mismo incremento
 * (ratio = 1.0f; última palabra común 0x00207A en 114). La primera diferencia es
 * 115 (LFO1=0x0021A0, LFO2=0x0020D4); LFO2 queda por debajo en 115..120,
 * vuelve a superarlo en 121 y alcanza una razón de 5.3239x en 127.
 */
inline float getHardwareLfo2SpeedRatio(float midiKnob0to127) noexcept
{
    const float clamped = std::clamp(midiKnob0to127, 0.0f, 127.0f);
    const int i0 = static_cast<int>(clamped);
    const int i1 = std::min(127, i0 + 1);
    const float frac = clamped - static_cast<float>(i0);

    const float r0 = static_cast<float>(kRomLfo2IncrementQ23[i0]) / static_cast<float>(kRomLfo1IncrementQ23[i0]);
    const float r1 = static_cast<float>(kRomLfo2IncrementQ23[i1]) / static_cast<float>(kRomLfo1IncrementQ23[i1]);
    return r0 + frac * (r1 - r0);
}

namespace detail
{
/**
 * @brief Frecuencia en Hz para un knob 0..127 sobre una tabla ROM de incrementos Q23.
 *        f_Hz = (inc / 2^24) * fs, interpolando el incremento entre entradas contiguas.
 */
inline float lfoFreqHzFromIncrements(const uint32_t* table, float midiKnob0to127) noexcept
{
    const float clamped = std::clamp(midiKnob0to127, 0.0f, 127.0f);
    const int i0 = static_cast<int>(clamped);
    const int i1 = std::min(127, i0 + 1);
    const float frac = clamped - static_cast<float>(i0);

    const double inc0 = static_cast<double>(table[i0]);
    const double inc1 = static_cast<double>(table[i1]);
    const double inc = inc0 + static_cast<double>(frac) * (inc1 - inc0);
    return static_cast<float>((inc / 16777216.0) * kHardwareSampleRate);
}
} // namespace detail

/**
 * @brief Frecuencia exacta en Hz de LFO1 según la tabla ROM 0x295E8 (DSP56362 P:$0500).
 *        El acumulador de fase de 24 bits con signo recorre 2 * 2^23 muestras por ciclo
 *        a fs = 48.000 kHz: f_Hz = (incQ23 / 2^24) * 48000.0 (0.0458 Hz .. 36.61 Hz).
 */
inline float getHardwareLfo1FreqHz(float midiKnob0to127) noexcept
{
    return detail::lfoFreqHzFromIncrements(kRomLfo1IncrementQ23, midiKnob0to127);
}

/**
 * @brief Frecuencia exacta en Hz de LFO2 según la tabla ROM 0x29BE8 (DSP56362 P:$052A).
 *        Idéntica a LFO1 entre knob 0..114 (0.0458 Hz .. 23.7865 Hz; última
 *        palabra común 0x00207A en 114); difiere primero en 115 y alcanza
 *        194.9043 Hz en 127 (5.3239x LFO1 en ese índice).
 */
inline float getHardwareLfo2FreqHz(float midiKnob0to127) noexcept
{
    return detail::lfoFreqHzFromIncrements(kRomLfo2IncrementQ23, midiKnob0to127);
}

// ============================================================================
// Resolucion de LUTs por `lutId` (contrato comun: ABDShared/Contracts/lut_tables.json)
//    El contrato declara las tablas de firmware y los parametros que las usan con
//    `curve: "Lut"` + `lutId` (CDD 4.5). `conversion::normalizedToDomain` y
//    `conversion::domainToNormalized` resuelven aqui la tabla en vez de aproximarla
//    con una formula: la entrada `i` de 128 corresponde al normalizado `i/127`, que
//    es exactamente el raw MIDI 0..127 del knob.
// ============================================================================
inline constexpr int kLutLength = 128;

/** @brief Tabla de incrementos Q23 del `lutId`, o nullptr si el id no existe. */
inline const uint32_t* getLutIncrementsQ23(const char* lutId) noexcept
{
    if (lutId == nullptr)
        return nullptr;
    if (std::strcmp(lutId, "ms2000_lfo1_hz") == 0)
        return kRomLfo1IncrementQ23;
    if (std::strcmp(lutId, "ms2000_lfo2_hz") == 0)
        return kRomLfo2IncrementQ23;
    return nullptr;
}

/**
 * @brief Normalizado 0..1 -> valor de la tabla en Hz, interpolando entre entradas.
 *        Devuelve NaN si el `lutId` no existe: nunca degrada a lineal en silencio.
 */
inline float getLutValueHz(const char* lutId, float normalized) noexcept
{
    const uint32_t* table = getLutIncrementsQ23(lutId);
    if (table == nullptr)
        return std::numeric_limits<float>::quiet_NaN();
    const float knob = std::clamp(normalized, 0.0f, 1.0f) * static_cast<float>(kLutLength - 1);
    return detail::lfoFreqHzFromIncrements(table, knob);
}

/**
 * @brief Inversa Hz -> normalizado 0..1. En una meseta (entradas contiguas con el
 *        mismo valor) devuelve el inicio de la meseta, igual que
 *        `lutNormalizedForValue` del registro compartido.
 */
inline float getLutNormalizedForHz(const char* lutId, float valueHz) noexcept
{
    const uint32_t* table = getLutIncrementsQ23(lutId);
    if (table == nullptr)
        return std::numeric_limits<float>::quiet_NaN();

    const float first = detail::lfoFreqHzFromIncrements(table, 0.0f);
    const float last = detail::lfoFreqHzFromIncrements(table, static_cast<float>(kLutLength - 1));
    if (valueHz <= first)
        return 0.0f;
    if (valueHz >= last)
        return 1.0f;

    float previous = first;
    for (int index = 1; index < kLutLength; ++index)
    {
        const float current = detail::lfoFreqHzFromIncrements(table, static_cast<float>(index));
        if (valueHz <= current)
        {
            if (current <= previous) // meseta: el valor ya existia en la entrada anterior
                return static_cast<float>(index - 1) / static_cast<float>(kLutLength - 1);
            const float fraction = (valueHz - previous) / (current - previous);
            return (static_cast<float>(index - 1) + fraction) / static_cast<float>(kLutLength - 1);
        }
        previous = current;
    }
    return 1.0f;
}

// 1.0 en el espacio de coeficientes del DSP (Q23 fraccional, 2^23 = 0x800000).
inline constexpr double kQ23FractionOne = 8388608.0;

/**
 * @brief Techo del coeficiente de corte del VCF en el DSP56362: `$770000` (`0.9297`),
 *        el límite que el microcódigo aplica en P:$0760..$0766 (`#>$77` en Q7).
 */
inline constexpr double kHardwareFilterMaxCoefficient = static_cast<double>(0x770000u) / kQ23FractionOne;

/**
 * @brief Frecuencia de corte exacta en Hz del VCF tal como la implementa el hardware.
 *
 * El H8S **no** envía el cutoff desde `kRomFilterCutoffQ23` (0x26154): el valor que escribe
 * al DSP es la tabla maestra de pitch, `kRomMasterPitchQ23[128 + knob]` (0x240F0).
 * Verificado por volcado de X: del DSP56362 en ms2000r-emu (7 knobs, 0x4B0 = word exacto,
 * idéntico en las 4 voces e independiente de la resonancia):
 *
 *     X:$04B0(voice i) = kRomMasterPitchQ23[128 + knob]
 *
 * El microcódigo (P:$0740..$078F) convierte ese word en el coeficiente de la cámara resonante
 * y lo recorta en `$770000` (`kHardwareFilterMaxCoefficient`, 0.9297). La relación exacta,
 * medida por autoscilación del filtro en el emulador (reso 127, cola tras note-off) en los
 * knobs 20/60/90/100/127, es un coeficiente tipo Chamberlin:
 *
 *     c = min(2 * w, 0.9297)      con w = word / 2^23
 *     c = sin(pi * fc / fs)       =>  fc = (fs / pi) * asin(c)
 *
 * Error de la ley frente a la autoscilación medida: 0.14 / 0.13 / 0.15 / 0.18 / 0.28 %
 * (207.5 / 2090 / 9348 / 13696 / 18186 Hz). Las leyes de tipo `tan` (pre-warping bilineal)
 * fallan en el extremo alto: `atan(2w)` queda 26 % bajo en el knob 127 porque el hardware
 * NO pre-warpéa, satura en 0.9297.
 *
 * Consecuencias de la ley:
 *  - knob 0 = 65.4 Hz, 1 semitono por paso hasta ~knob 72 (ahí empieza la compresión de la sin).
 *  - El techo de 0.9297 se alcanza en el knob ~109: de ahí en adelante fc = 18,193 Hz.
 *  - Por debajo del knob 0 la misma tabla (índices 0..127) sigue bajando hasta ~0.2 Hz para
 *    modulación negativa.
 *
 * `modOctaves` replica el bloque `P:$0749..$0768`:
 *  - `X:$04B0` (`wStatic`) se obtiene de `kRomMasterPitchQ23[128 + knob + kbdSemis]` (`H8S 0x13390`).
 *  - La modulación dinámica (`EG1`, `VirtualPatch`, `FilterVelo`) se recorta a `[-8.0, +8.0]` octavas
 *    (`P:$074D..$074F`: `add #$40,a` + clamp `[0, $7FFFFF]`) y se convierte en el multiplicador lineal
 *    `2^modOctaves` (`P:$0751..$075B`) que multiplica a `wStatic` (`P:$075E..$075F`) antes de saturar en `$770000`.
 */
inline float getHardwareFilterCutoffHz(float cutoffNorm0to1,
                                       float modOctaves = 0.0f,
                                       float w3Scale = 1.0f,
                                       float kbdOctaves = 0.0f) noexcept
{
    const float knob0to127 = std::clamp(cutoffNorm0to1, 0.0f, 1.0f) * 127.0f;
    const float staticRomIndex = std::clamp(128.0f + knob0to127 + (kbdOctaves * 12.0f), 0.0f, 255.0f);
    const int i0 = static_cast<int>(staticRomIndex);
    const int i1 = std::min(255, i0 + 1);
    const float frac = staticRomIndex - static_cast<float>(i0);

    const double w0 = static_cast<double>(kRomMasterPitchQ23[i0]);
    const double w1 = static_cast<double>(kRomMasterPitchQ23[i1]);
    const double wStatic = (w0 + static_cast<double>(frac) * (w1 - w0)) / kQ23FractionOne;

    const double clampedModOctaves = std::clamp(static_cast<double>(modOctaves), -8.0, 8.0);
    const double w = wStatic * std::exp2(clampedModOctaves);

    const double c = std::min(2.0 * w, kHardwareFilterMaxCoefficient) * static_cast<double>(w3Scale);
    const double hz = (static_cast<double>(kHardwareSampleRate) / 3.14159265358979323846) * std::asin(std::clamp(c, 0.0, 0.999));
    return static_cast<float>(std::clamp(hz, 5.0, 20000.0));
}

// ============================================================================
// 5a-bis. Tablas de Descriptores por Modo del VCF (ROM 0x25DF4 / 0x25ECC / 0x25FA4 / 0x2607C)
//         y Curva Bipolar de EG1 Intensity / Pan (ROM 0x2BD1C)
//         En H8S 0x16106..0x16274, el selector Filter Type (0..3) recorre 36 entradas
//         de 6 bytes (`216 B` por tabla = 9 registros × 4 voces), donde cada entrada es:
//           - +0x00..0x01: dirección `uint16_t` en memoria X del DSP56362 (big-endian)
//           - +0x02..0x04: palabra `Q23` de 24 bits con signo (big-endian)
//           - +0x05:       `0x00` (byte de padding del longword de 32 bits)
//         Las 9 entradas de la Voz 0 (replicadas en las Voces 1..3) son exactamente:
//           [0] +0x00 -> X:$04E3 (puntero Y de salida de Etapa 1: $0424 LP1 / $0425 BP1 / $0426 HP1)
//           [1] +0x06 -> X:$04E4 (puntero Y de salida de Etapa 2: $0427 LP2 / $0428 BP2 / $0429 HP2)
//           [2] +0x0C -> X:$04B2 (mezcla etapa 1/2 y flag de saturación post-SVF en P:$083F..$0842)
//           [3] +0x12 -> X:$04E5 ($000000)
//           [4] +0x18 -> X:$04E6 (ganancia post-SVF que multiplica a w0: $200000 / $400000 / $600000 / $47B000)
//           [5] +0x1E -> X:$04DC (puntero Y de entrada a Etapa 2: $0208 en 24dB/BPF/HPF, $0424 en 12LPF)
//           [6] +0x24 -> X:$0524 (coeficiente a1/8 del filtro de salida en P:$086D..$0890: $044985 o $000000)
//           [7] +0x2A -> X:$0526 (coeficiente b1/8 del filtro de salida: $EC6079 = -1285991 o $000000)
//           [8] +0x30 -> X:$0528 (coeficiente b0/8 del filtro de salida: $1F5601 o $100000 = 0.125 -> x8 = 1.0)
// ============================================================================
struct HardwareFilterModeCoeffs
{
    uint16_t stage1TapYAddr;        ///< Entry 0 (+0x00) -> X:$04E3 ($0424 LP1, $0425 BP1, $0426 HP1)
    uint16_t stage2TapYAddr;        ///< Entry 1 (+0x06) -> X:$04E4 ($0427 LP2, $0428 BP2, $0429 HP2)
    int32_t stageMixAndSatFlagQ23;  ///< Entry 2 (+0x0C) -> X:$04B2 (>= 0 activa saturador post-SVF P:$083F..$0842; < 0 lo puentea)
    int32_t stageAuxQ23;            ///< Entry 3 (+0x12) -> X:$04E5 ($000000)
    uint32_t postGainScaleQ23;      ///< Entry 4 (+0x18) -> X:$04E6 (multiplica a w0 en P:$0771: X:$04E8 = 2 * X:$04E6 * w0)
    uint16_t stage2InputYAddr;      ///< Entry 5 (+0x1E) -> X:$04DC ($0208 o $0424)
    int32_t postFilterA1Q23;        ///< Entry 6 (+0x24) -> X:$0524 ($044985 en LPF; $000000 en BPF/HPF)
    int32_t postFilterB1Q23;        ///< Entry 7 (+0x2A) -> X:$0526 ($EC6079 = -1285991 en LPF; $000000 en BPF/HPF)
    int32_t postFilterB0Q23;        ///< Entry 8 (+0x30) -> X:$0528 ($1F5601 en LPF; $100000 = 0.125 -> x8 = 1.0 en BPF/HPF)
    bool postSaturationEnabled;     ///< true cuando X:$04B2 >= 0 (24LPF/12LPF); false cuando X:$04B2 = $800001 (12BPF/12HPF)
};

inline constexpr HardwareFilterModeCoeffs kRomFilterModeDescriptors[4] = {
    // 0: 24LPF (ROM 0x25DF4)
    {0x0424u, 0x0427u, 0x000000, 0x000000, 0x200000u, 0x0208u, 0x044985, -1285991, 0x1F5601, true},
    // 1: 12LPF (ROM 0x25ECC)
    {0x0424u, 0x0427u, 0x7FFFFF, 0x000000, 0x400000u, 0x0424u, 0x044985, -1285991, 0x1F5601, true},
    // 2: 12BPF (ROM 0x25FA4, X:$04B2 = $800001 = -8388607)
    {0x0425u, 0x0428u, -8388607, 0x000000, 0x600000u, 0x0208u, 0x000000, 0x000000, 0x100000, false},
    // 3: 12HPF (ROM 0x2607C, X:$04B2 = $800001 = -8388607)
    {0x0426u, 0x0429u, -8388607, 0x000000, 0x47B000u, 0x0208u, 0x000000, 0x000000, 0x100000, false},
};

inline constexpr int32_t kRomSignedLinearIntensityQ23[128] = {
    -8388607, -8388607, -8255455, -8122302, -7989150, -7855997, -7722845, -7589692, -7456540, -7323387, -7190235,
    -7057082, -6923930, -6790777, -6657625, -6524472, -6391320, -6258167, -6125015, -5991862, -5858710, -5725557,
    -5592405, -5459252, -5326100, -5192947, -5059795, -4926642, -4793490, -4660337, -4527185, -4394032, -4260880,
    -4127727, -3994575, -3861422, -3728270, -3595117, -3461965, -3328812, -3195660, -3062507, -2929355, -2796202,
    -2663050, -2529897, -2396745, -2263592, -2130440, -1997287, -1864135, -1730982, -1597830, -1464677, -1331525,
    -1198372, -1065220, -932067,  -798915,  -665762,  -532610,  -399457,  -266305,  -133152,  0,        133152,
    266305,   399457,   532610,   665762,   798915,   932067,   1065220,  1198372,  1331525,  1464677,  1597830,
    1730982,  1864135,  1997287,  2130440,  2263592,  2396745,  2529897,  2663050,  2796202,  2929355,  3062507,
    3195660,  3328812,  3461965,  3595117,  3728270,  3861422,  3994575,  4127727,  4260880,  4394032,  4527185,
    4660337,  4793490,  4926642,  5059795,  5192947,  5326100,  5459252,  5592405,  5725557,  5858710,  5991862,
    6125015,  6258167,  6391320,  6524472,  6657625,  6790777,  6923930,  7057082,  7190235,  7323387,  7456540,
    7589692,  7722845,  7855997,  7989150,  8122302,  8255455,  8388607,
};

/**
 * @brief Devuelve el coeficiente X:$04AF (en rango [-0.5, +0.5]) para Filter EG1 Intensity,
 *        reproduciendo la lectura de 0x2BD1C seguida de `shar.l #1, er0` en H8S 0x0166BE.
 */
inline float getHardwareFilterEg1IntensityCoeff(float normMinus1To1) noexcept
{
    const float rawIdx = std::clamp((normMinus1To1 * 63.0f) + 64.0f, 1.0f, 127.0f);
    const int i0 = static_cast<int>(rawIdx);
    const int i1 = std::min(127, i0 + 1);
    const float frac = rawIdx - static_cast<float>(i0);
    const double v0 = static_cast<double>(kRomSignedLinearIntensityQ23[i0] >> 1);
    const double v1 = static_cast<double>(kRomSignedLinearIntensityQ23[i1] >> 1);
    const double v = (v0 + static_cast<double>(frac) * (v1 - v0)) / kQ23FractionOne;
    return static_cast<float>(std::clamp(v, -0.5, 0.5));
}

/**
 * @brief Devuelve el factor de ganancia post-SVF por modo (`2 * X:$04E6`) que multiplica a `w0`
 *        en `P:$077B` (`mpy x1,x0,a` -> `X:$04E8`) y `P:$07B4..$07B5` (`mpy y0,x0,a` / `asl a`):
 *          - 0 (`24LPF`): `2 * ($200000 / 2^23) = 0.50`
 *          - 1 (`12LPF`): `2 * ($400000 / 2^23) = 1.00`
 *          - 2 (`12BPF`): `2 * ($600000 / 2^23) = 1.50`
 *          - 3 (`12HPF`): `2 * ($47B000 / 2^23) = 1.120117`
 */
inline constexpr float getHardwareFilterModePostGain(int filterTypeIndex) noexcept
{
    const int idx = (filterTypeIndex >= 0 && filterTypeIndex < 4) ? filterTypeIndex : 0;
    return static_cast<float>((2.0 * static_cast<double>(kRomFilterModeDescriptors[idx].postGainScaleQ23)) /
                              kQ23FractionOne);
}

/**
 * @brief Coeficientes escalados (`x8` desde `P:$087F`: `asl #$03,a,a`) del filtro de salida de voz
 *        de 1.er orden (`P:$086D..$0890`, `X:$0524..$0528`):
 *          `y[n] = b0 * x[n] + b1 * x[n-1] + a1 * y[n-1]`
 *        - En `24LPF`/`12LPF`: `b0 = 8 * $1F5601 = +1.95850`, `b1 = 8 * $EC6079 = -1.22645`, `a1 = 8 * $044985 = +0.26795`
 *          (ganancia DC exacta `= (1.95850 - 1.22645) / (1 - 0.26795) = 0.73205 / 0.73205 = 1.0000`,
 *          y realce de agudos `+12.9 dB` en Nyquist que compensa la caída del ZOH/SVF).
 *        - En `12BPF`/`12HPF`: `b0 = 8 * $100000 = 1.0`, `b1 = 0.0`, `a1 = 0.0` (bypass plano de ganancia unidad).
 */
struct HardwareVoiceOutputFilterCoeffs
{
    float b0;
    float b1;
    float a1;
};

inline constexpr HardwareVoiceOutputFilterCoeffs getHardwareVoiceOutputFilterCoeffs(
    int filterTypeIndex, double sampleRate = kHardwareSampleRate) noexcept
{
    const int idx = (filterTypeIndex >= 0 && filterTypeIndex < 4) ? filterTypeIndex : 0;
    const auto &desc = kRomFilterModeDescriptors[idx];
    const double b0_48k = (8.0 * static_cast<double>(desc.postFilterB0Q23)) / kQ23FractionOne;
    const double b1_48k = (8.0 * static_cast<double>(desc.postFilterB1Q23)) / kQ23FractionOne;
    const double a1_48k = (8.0 * static_cast<double>(desc.postFilterA1Q23)) / kQ23FractionOne;

    // En 12BPF/12HPF (a1 = 0, b1 = 0) o exactamente a 48 kHz devolvemos los coeficientes nativos de ROM.
    if (desc.postFilterA1Q23 == 0 || std::abs(sampleRate - static_cast<double>(kHardwareSampleRate)) < 1.0)
    {
        return {static_cast<float>(b0_48k), static_cast<float>(b1_48k), static_cast<float>(a1_48k)};
    }

    // Re-mapeo de polo/cero a la frecuencia de muestreo del host (P3.5) conservando ganancia DC = 1.0000
    // y ganancia de alta frecuencia (g_inf = (b0 - b1) / (1 + a1)) mediante transformada bilineal:
    //   polo continuo: K_p = (1 - a1_48k) / (1 + a1_48k) * (48000 / fs)  =>  a1(fs) = (1 - K_p) / (1 + K_p)
    const double fsRatio = static_cast<double>(kHardwareSampleRate) / std::max(1000.0, sampleRate);
    const double kp = ((1.0 - a1_48k) / (1.0 + a1_48k)) * fsRatio;
    const double a1Fs = std::clamp((1.0 - kp) / (1.0 + kp), -0.995, 0.995);
    const double gInf = (b0_48k - b1_48k) / (1.0 + a1_48k);
    const double sumB = 1.0 - a1Fs;          // b0 + b1 = 1 - a1 (ganancia DC = 1.0000)
    const double diffB = gInf * (1.0 + a1Fs);  // b0 - b1 = g_inf * (1 + a1)
    return {static_cast<float>(0.5 * (sumB + diffB)),
            static_cast<float>(0.5 * (sumB - diffB)),
            static_cast<float>(a1Fs)};
}

// ============================================================================
// 5a-ter. Tabla de Nivel de Mezclador / Amp Level (ROM 0x2C31C, 128 × 4B Q23)
//         Usada en H8S 0x15BAE (OSC1 Level), 0x15DD8 (OSC2 Level), 0x15FDC (Noise Level)
//         y 0x166EA / 0x13562 (Amp Level). Presenta una ley ligeramente cóncava
//         (knob 64 -> 3388520 / 8388607 = 0.4039, knob 100 -> 0.7204, knob 127 -> 1.0).
// ============================================================================
inline constexpr int32_t kRomMixerAmpCurveQ23[128] = {
    0,       39839,   80094,   120765,  161853,  203357,  245276,  287612,  330364,  373531,  417115,  461115,
    505532,  550364,  595612,  641276,  687357,  733853,  780766,  828094,  875839,  924000,  972577,  1021569,
    1070978, 1120804, 1171045, 1221702, 1272775, 1324265, 1376170, 1428492, 1481229, 1534383, 1587953, 1641938,
    1696340, 1751158, 1806392, 1862042, 1918109, 1974591, 2031489, 2088804, 2146534, 2204681, 2263243, 2322222,
    2381617, 2441428, 2501655, 2562298, 2623357, 2684832, 2746723, 2809031, 2871754, 2934894, 2998449, 3062421,
    3126809, 3191612, 3256832, 3322468, 3388520, 3454988, 3521873, 3589173, 3656889, 3725022, 3793570, 3862535,
    3931915, 4001712, 4071925, 4142554, 4213599, 4285060, 4356937, 4429230, 4501939, 4575064, 4648606, 4722563,
    4796937, 4871726, 4946932, 5022554, 5098592, 5175046, 5251916, 5329202, 5406904, 5485022, 5563556, 5642507,
    5721873, 5801656, 5881854, 5962469, 6043500, 6124946, 6206809, 6289088, 6371783, 6454895, 6538422, 6622365,
    6706724, 6791500, 6876691, 6962299, 7048323, 7134762, 7221618, 7308890, 7396578, 7484682, 7573202, 7662138,
    7751491, 7841259, 7931443, 8022044, 8113060, 8204493, 8296342, 8388607,
};

/**
 * @brief Evalúa con interpolación lineal la curva de nivel `0x2C31C` (`[0.0, 1.0]`).
 */
inline float getHardwareMixerAmpGain(float levelNorm0to1) noexcept
{
    const float knob = std::clamp(levelNorm0to1, 0.0f, 1.0f) * 127.0f;
    const int i0 = static_cast<int>(knob);
    const int i1 = std::min(127, i0 + 1);
    const float frac = knob - static_cast<float>(i0);
    const double v0 = static_cast<double>(kRomMixerAmpCurveQ23[i0]);
    const double v1 = static_cast<double>(kRomMixerAmpCurveQ23[i1]);
    return static_cast<float>((v0 + static_cast<double>(frac) * (v1 - v0)) / kQ23FractionOne);
}

// ============================================================================
// 5b. Tablas de Resonancia y Compensación del VCF (ROM 0x26554 y 0x26D54)
//     En H8S 0x16460..0x165E0, el parámetro Resonance (0..127) indexa con
//     stride 16 (4 palabras Q23 de 24 bits: w0, w1, w2, w3):
//       - 0x26554 (128 × 16 B): usada cuando filterType == 0 (24LPF).
//       - 0x26D54 (128 × 16 B): usada cuando filterType != 0 (12LPF, 12BPF, 12HPF).
//     Las columnas w1, w2 y w3 son idénticas bit a bit en ambas tablas; sólo
//     difiere w0 (escalado de salida post-SVF enviado a X:$04E7 vía 0x116B4).
//     En el DSP56362 (P:$0768..$07B7):
//       - w0 -> X:$04E7 (ganancia post-SVF: 2 * X:$04E6 * w0)
//       - w1 -> X:$04DD, X:$04E0 (ganancia de entrada u_i * w1 en cada etapa SVF)
//       - w2 -> X:$04DE, X:$04E1 (realimentación BP con signo: 2*R = 1 - w2, Q_etapa = 1/(1 - w2))
//       - w3 -> X:$04B1 (compensación de frecuencia de corte: g = 2 * w3 * c)
// ============================================================================
inline constexpr uint32_t kRomFilterResW0_24LPF_Q23[128] = {
    0x2A744Eu, 0x2A7404u, 0x2A73CEu, 0x2A73AEu, 0x2A73A3u, 0x2A7668u, 0x2A7EA4u, 0x2A8C3Cu, 0x2A9F20u, 0x2AB741u,
    0x2AD498u, 0x2AF721u, 0x2B1EDFu, 0x2B4BD6u, 0x2B7E10u, 0x2BB59Cu, 0x2BF289u, 0x2C34EBu, 0x2C7CDBu, 0x2CCA71u,
    0x2D1DCAu, 0x2D7706u, 0x2DD645u, 0x2E3BACu, 0x2EA75Eu, 0x2F1985u, 0x2F9247u, 0x3011D0u, 0x30984Au, 0x3125E1u,
    0x31BAC4u, 0x325720u, 0x32FB22u, 0x33A6FAu, 0x345AD7u, 0x3516E6u, 0x35DB57u, 0x36A857u, 0x377E13u, 0x385CB7u,
    0x39446Eu, 0x3A3562u, 0x3B2FBBu, 0x3C339Fu, 0x3D4131u, 0x3E5895u, 0x3F79E9u, 0x40A54Au, 0x41DAD2u, 0x431A98u,
    0x4464AEu, 0x45B924u, 0x471806u, 0x48815Cu, 0x49F52Au, 0x4B736Eu, 0x4CFC24u, 0x4E8F41u, 0x502CB6u, 0x51D470u,
    0x538654u, 0x554245u, 0x57081Du, 0x58D7B4u, 0x5AB0DAu, 0x5C9358u, 0x5E7EF5u, 0x60736Fu, 0x627080u, 0x6475D9u,
    0x668329u, 0x689817u, 0x6AB443u, 0x6CD749u, 0x6F00BEu, 0x713032u, 0x73652Fu, 0x759F3Au, 0x77DDD2u, 0x7A2070u,
    0x7C668Au, 0x7EAF8Fu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
};

inline constexpr uint32_t kRomFilterResW0_12dB_Q23[128] = {
    0x20DA95u, 0x20DA89u, 0x20DA80u, 0x20DA7Bu, 0x20DA79u, 0x20DAEDu, 0x20DC46u, 0x20DE82u, 0x20E1A2u, 0x20E5A8u,
    0x20EA98u, 0x20F07Au, 0x20F755u, 0x20FF35u, 0x210826u, 0x211238u, 0x211D7Au, 0x212A01u, 0x2137E2u, 0x214735u,
    0x215813u, 0x216A9Bu, 0x217EECu, 0x219529u, 0x21AD77u, 0x21C800u, 0x21E4EFu, 0x220476u, 0x2226C7u, 0x224C1Bu,
    0x2274ACu, 0x22A0BCu, 0x22D08Eu, 0x23046Cu, 0x233CA3u, 0x237987u, 0x23BB70u, 0x2402BBu, 0x244FCBu, 0x24A307u,
    0x24FCDFu, 0x255DC6u, 0x25C634u, 0x2636AAu, 0x26AFABu, 0x2731C3u, 0x27BD81u, 0x28537Au, 0x28F44Au, 0x29A090u,
    0x2A58F1u, 0x2B1E14u, 0x2BF0A8u, 0x2CD15Cu, 0x2DC0E4u, 0x2EBFF6u, 0x2FCF49u, 0x30EF97u, 0x322198u, 0x336606u,
    0x34BD98u, 0x362903u, 0x37A8FAu, 0x393E29u, 0x3AE93Au, 0x3CAACDu, 0x3E837Cu, 0x4073D8u, 0x427C67u, 0x449DA4u,
    0x46D7FCu, 0x492BCFu, 0x4B996Cu, 0x4E2111u, 0x50C2EBu, 0x537F12u, 0x56558Au, 0x594640u, 0x5C510Bu, 0x5F75A7u,
    0x62B3B9u, 0x660ACBu, 0x697A4Cu, 0x6D018Eu, 0x709FC8u, 0x745411u, 0x781D67u, 0x7BFAA7u, 0x7FEA91u, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
    0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu, 0x7FFFFFu,
};

inline constexpr uint32_t kRomFilterResW1_InputGainQ23[128] = {
    0x401EFAu, 0x401F6Cu, 0x401FBEu, 0x401FEFu, 0x401FFFu, 0x401BC4u, 0x400F37u, 0x3FFA8Bu, 0x3FDDF5u, 0x3FB9A8u,
    0x3F8DD6u, 0x3F5AB2u, 0x3F206Fu, 0x3EDF3Eu, 0x3E974Fu, 0x3E48D4u, 0x3DF3FEu, 0x3D98FCu, 0x3D37FDu, 0x3CD131u,
    0x3C64C6u, 0x3BF2EBu, 0x3B7BCDu, 0x3AFF9Au, 0x3A7E7Fu, 0x39F8A7u, 0x396E41u, 0x38DF76u, 0x384C72u, 0x37B560u,
    0x371A6Au, 0x367BBBu, 0x35D97Cu, 0x3533D6u, 0x348AF1u, 0x33DEF7u, 0x33300Eu, 0x327E5Eu, 0x31CA0Fu, 0x311346u,
    0x305A2Au, 0x2F9EE0u, 0x2EE18Fu, 0x2E2259u, 0x2D6165u, 0x2C9ED7u, 0x2BDAD0u, 0x2B1576u, 0x2A4EEAu, 0x29874Eu,
    0x28BEC6u, 0x27F571u, 0x272B72u, 0x2660E8u, 0x2595F4u, 0x24CAB6u, 0x23FF4Du, 0x2333D7u, 0x226874u, 0x219D41u,
    0x20D25Bu, 0x2007E1u, 0x1F3DEFu, 0x1E74A0u, 0x1DAC12u, 0x1CE45Fu, 0x1C1DA3u, 0x1B57F8u, 0x1A9378u, 0x19D03Eu,
    0x190E62u, 0x184DFDu, 0x178F28u, 0x16D1FBu, 0x16168Eu, 0x155CF8u, 0x14A54Fu, 0x13EFAAu, 0x133C1Fu, 0x128AC3u,
    0x11DBACu, 0x112EEEu, 0x10849Du, 0x0FDCCDu, 0x0F3792u, 0x0E94FFu, 0x0DF525u, 0x0D5818u, 0x0CBDE8u, 0x0C26A8u,
    0x0B9267u, 0x0B0136u, 0x0A7326u, 0x09E846u, 0x0960A4u, 0x08DC50u, 0x085B58u, 0x07DDC9u, 0x0763B2u, 0x06ED1Eu,
    0x067A1Cu, 0x060AB6u, 0x059EF8u, 0x0536EEu, 0x04D2A3u, 0x047221u, 0x041572u, 0x03BC9Fu, 0x0367B3u, 0x0316B4u,
    0x02C9ADu, 0x0280A4u, 0x023BA2u, 0x01FAACu, 0x01BDCAu, 0x018502u, 0x01505Au, 0x011FD7u, 0x00F37Eu, 0x00CB52u,
    0x00A75Au, 0x008797u, 0x006C0Du, 0x0054BFu, 0x0041B0u, 0x0032E0u, 0x002853u, 0x002208u,
};

inline constexpr int32_t kRomFilterResW2_FeedbackQ23[128] = {
    -4194303, -3145727, -2097151, -1048575, 0,       134780,  268469,  401066,  532571,  662986,  792308,  920539,
    1047679,  1173727,  1298683,  1422548,  1545321, 1667003, 1787593, 1907092, 2025499, 2142814, 2259038, 2374171,
    2488212,  2601161,  2713019,  2823785,  2933460, 3042043, 3149534, 3255934, 3361243, 3465460, 3568585, 3670619,
    3771561,  3871412,  3970171,  4067839,  4164415, 4259900, 4354293, 4447594, 4539804, 4630922, 4720949, 4809885,
    4897728,  4984480,  5070141,  5154710,  5238188, 5320574, 5401868, 5482071, 5561182, 5639202, 5716130, 5791967,
    5866712,  5940366,  6012928,  6084398,  6154777, 6224064, 6292260, 6359364, 6425377, 6490298, 6554128, 6616866,
    6678513,  6739068,  6798531,  6856903,  6914183, 6970372, 7025469, 7079475, 7132389, 7184212, 7234943, 7284582,
    7333130,  7380587,  7426951,  7472225,  7516406, 7559497, 7601495, 7642402, 7682218, 7720942, 7758574, 7795115,
    7830565,  7864923,  7898189,  7930363,  7961447, 7991438, 8020338, 8048147, 8074864, 8100489, 8125023, 8148465,
    8170816,  8192075,  8212243,  8231319,  8249304, 8266197, 8281998, 8296708, 8310327, 8322853, 8334289, 8344632,
    8353885,  8362045,  8369114,  8375092,  8379978, 8383772, 8386475, 8388086,
};

inline constexpr uint32_t kRomFilterResW3_CutoffCompQ23[128] = {
    0x3FFFFFu, 0x41FFFFu, 0x43FFFFu, 0x45FFFFu, 0x47FFFEu, 0x48DB68u, 0x49B520u, 0x4A8D27u, 0x4B637Du, 0x4C3821u,
    0x4D0B15u, 0x4DDC57u, 0x4EABE9u, 0x4F79C9u, 0x5045F8u, 0x511075u, 0x51D942u, 0x52A05Du, 0x5365C8u, 0x542981u,
    0x54EB89u, 0x55ABE0u, 0x566A85u, 0x57277Au, 0x57E2BDu, 0x589C4Fu, 0x595430u, 0x5A0A60u, 0x5ABEDFu, 0x5B71ADu,
    0x5C22C9u, 0x5CD234u, 0x5D7FEFu, 0x5E2BF8u, 0x5ED64Fu, 0x5F7EF6u, 0x6025ECu, 0x60CB30u, 0x616EC3u, 0x6210A5u,
    0x62B0D6u, 0x634F56u, 0x63EC24u, 0x648742u, 0x6520AEu, 0x65B869u, 0x664E73u, 0x66E2CCu, 0x677574u, 0x68066Au,
    0x6895B0u, 0x692344u, 0x69AF27u, 0x6A3959u, 0x6AC1D9u, 0x6B48A9u, 0x6BCDC7u, 0x6C5135u, 0x6CD2F1u, 0x6D52FCu,
    0x6DD156u, 0x6E4DFEu, 0x6EC8F6u, 0x6F423Cu, 0x6FB9D1u, 0x702FB5u, 0x70A3E8u, 0x71166Au, 0x71873Bu, 0x71F65Au,
    0x7263C8u, 0x72CF85u, 0x733991u, 0x73A1ECu, 0x740896u, 0x746D8Eu, 0x74D0D6u, 0x75326Cu, 0x759251u, 0x75F085u,
    0x764D07u, 0x76A7D9u, 0x7700F9u, 0x775869u, 0x77AE27u, 0x780234u, 0x78548Fu, 0x78A53Au, 0x78F434u, 0x79417Cu,
    0x798D13u, 0x79D6F9u, 0x7A1F2Eu, 0x7A65B2u, 0x7AAA84u, 0x7AEDA6u, 0x7B2F16u, 0x7B6ED5u, 0x7BACE3u, 0x7BE940u,
    0x7C23EBu, 0x7C5CE6u, 0x7C942Fu, 0x7CC9C7u, 0x7CFDAEu, 0x7D2FE4u, 0x7D6069u, 0x7D8F3Cu, 0x7DBC5Fu, 0x7DE7D0u,
    0x7E1190u, 0x7E399Fu, 0x7E5FFDu, 0x7E84A9u, 0x7EA7A5u, 0x7EC8EFu, 0x7EE888u, 0x7F0670u, 0x7F22A7u, 0x7F3D2Du,
    0x7F5602u, 0x7F6D25u, 0x7F8297u, 0x7F9658u, 0x7FA868u, 0x7FB8C7u, 0x7FC775u, 0x7FD471u,
};

struct HardwareFilterResonanceCoeffs
{
    float w0;          ///< Escalado post-SVF X:$04E7 (0.3317..1.0 en 24LPF; 0.2567..1.0 en 12dB)
    float w1;          ///< Ganancia de entrada por etapa SVF X:$04DD / X:$04E0 (0.5010..0.00104)
    float w2;          ///< Realimentación BP con signo X:$04DE / X:$04E1 (-0.5000..+0.99994)
    float w3;          ///< Compensación de frecuencia de corte con resonancia X:$04B1 (0.5000..0.9987)
    float dampingR;    ///< Amortiguamiento SVF R = (1 - w2) / 2 (0.7500..0.00003)
    float stageQ;      ///< Factor de calidad por etapa Q = 1 / (2 * R) = 1 / (1 - w2)
};

/**
 * @brief Devuelve los 4 coeficientes normalizados (w0, w1, w2, w3) y el amortiguamiento R
 *        extraídos de las tablas ROM 0x26554 (24LPF) y 0x26D54 (12dB) del Korg MS2000.
 */
inline HardwareFilterResonanceCoeffs getHardwareFilterResonanceCoeffs(float resNorm0to1, bool is24LPF = true) noexcept
{
    const float knob0to127 = std::clamp(resNorm0to1, 0.0f, 1.0f) * 127.0f;
    const int i0 = static_cast<int>(knob0to127);
    const int i1 = std::min(127, i0 + 1);
    const float frac = knob0to127 - static_cast<float>(i0);

    const uint32_t *w0Table = is24LPF ? kRomFilterResW0_24LPF_Q23 : kRomFilterResW0_12dB_Q23;
    const double w0 = (static_cast<double>(w0Table[i0]) +
                       static_cast<double>(frac) * (static_cast<double>(w0Table[i1]) - static_cast<double>(w0Table[i0]))) /
                      kQ23FractionOne;
    const double w1 = (static_cast<double>(kRomFilterResW1_InputGainQ23[i0]) +
                       static_cast<double>(frac) * (static_cast<double>(kRomFilterResW1_InputGainQ23[i1]) -
                                                    static_cast<double>(kRomFilterResW1_InputGainQ23[i0]))) /
                      kQ23FractionOne;
    const double w2 = (static_cast<double>(kRomFilterResW2_FeedbackQ23[i0]) +
                       static_cast<double>(frac) * (static_cast<double>(kRomFilterResW2_FeedbackQ23[i1]) -
                                                    static_cast<double>(kRomFilterResW2_FeedbackQ23[i0]))) /
                      kQ23FractionOne;
    const double w3 = (static_cast<double>(kRomFilterResW3_CutoffCompQ23[i0]) +
                       static_cast<double>(frac) * (static_cast<double>(kRomFilterResW3_CutoffCompQ23[i1]) -
                                                    static_cast<double>(kRomFilterResW3_CutoffCompQ23[i0]))) /
                      kQ23FractionOne;

    const float twoR = std::max(1.0e-5f, static_cast<float>(1.0 - w2));
    return {static_cast<float>(w0),
            static_cast<float>(w1),
            static_cast<float>(w2),
            static_cast<float>(w3),
            0.5f * twoR,
            1.0f / twoR};
}

// ============================================================================
// 6. Tablas de Escalas / Temperamentos Microtonales (ROM 0x13914 y 0x13980)
//    Punteros en ROM 0x13A20 (tabla de grados) y 0x139F8 (compensación de A4=440Hz por tonalidad).
//    Escalas 0..8:
//      0: Equal Temp   (0x13914)
//      1: Pure Major   (0x13920)
//      2: Pure Minor   (0x1392C)
//      3: Arabic       (0x13938) — E y B en cuarto de tono (-52 cents)
//      4: Pythagoras   (0x13944)
//      5: Werckmeister (0x13950) — Werckmeister III
//      6: Kirnberger   (0x1395C) — Kirnberger III
//      7: Slendro      (0x13968) — Gamelán pentatónico
//      8: Pelog        (0x13974) — Gamelán heptatónico
// ============================================================================
inline constexpr int8_t kRomScaleNoteCents[9][12] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                 // 0: Equal Temp
    {16, -14, 20, 31, 2, 14, -16, 18, -12, 0, 34, 4},     // 1: Pure Major
    {16, 49, 20, 31, 2, 14, 47, 18, 30, 0, 34, 4},        // 2: Pure Minor
    {0, 0, 0, 0, -52, 0, 0, 0, 0, 0, 0, -52},             // 3: Arabic
    {0, 14, 4, -6, 8, -2, 12, 2, 16, 6, -4, 10},          // 4: Pythagoras
    {0, -10, -8, -6, -10, -2, -12, -4, -8, -12, -4, -8},  // 5: Werckmeister
    {0, -9, -7, -6, -13, -2, -9, -3, -8, -10, -4, -12},   // 6: Kirnberger
    {0, 0, 63, 0, 0, -14, 0, 39, 0, 75, 0, 0},            // 7: Slendro
    {0, 0, -33, 0, 12, 37, 0, -17, 0, 35, 0, 0}           // 8: Pelog
};

inline constexpr int8_t kRomScaleKeyRefCents[9][12] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},              // 0: Equal Temp
    {0, -12, 18, -16, 14, 2, 31, 20, -14, 16, 4, 34},  // 1: Pure Major (mantiene A4 = 440 Hz)
    {0, 30, 18, 47, 14, 2, 31, 20, 49, 16, 4, 34},     // 2: Pure Minor (mantiene A4 = 440 Hz)
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

/**
 * @brief Devuelve la desviación de afinación en semitonos (-1.0 .. +1.0) para una nota MIDI
 *        según las tablas de temperamento en ROM (0x13914 / 0x13980).
 */
inline float getHardwareScaleDetuneSemitones(int midiNote, int scaleType, int scaleKey) noexcept
{
    if (scaleType <= 0 || scaleType > 8)
        return 0.0f;
    const int key = std::clamp(scaleKey, 0, 11);
    const int noteClass = ((midiNote % 12) + 12) % 12;
    const int degree = (noteClass - key + 12) % 12;
    const int cents = static_cast<int>(kRomScaleNoteCents[scaleType][degree])
                      - static_cast<int>(kRomScaleKeyRefCents[scaleType][key]);
    return static_cast<float>(cents) * 0.01f;
}

// ============================================================================
// 7. Tablas de Intensidad de Virtual Patch (ROM 0x2C71C y 0x2C11C, H8S 0x19B7E..0x19EB0)
//    - `kRomPatchPitchQ23` (0x2C71C): Curva por tramos para Pitch y OSC2Pitch (±1572864 = ±24 st).
//      Permite ajuste fino en cents para valores pequeños (±1..±6 -> ±1..±6 cents,
//      ±12 -> ±0.5 st, ±24 -> ±3 st, ±36 -> ±6 st, ±48 -> ±12 st, ±63 -> ±24 st).
//    - `kRomPatchSignedCurveQ23` (0x2C11C): Curva cuadrática con signo (±8388607 = ±1.0)
//      usada por el firmware para Cutoff, Amp, OSC1Ctrl1, Noise y LFO2Freq.
// ============================================================================
inline constexpr int32_t kRomPatchPitchQ23[128] = {
    -1572864, -1572864, -1520435, -1468006, -1415577, -1363148, -1310720, -1258291, -1205862, -1153433, -1101004,
    -1048576, -996147,  -943718,  -891289,  -838860,  -786432,  -753664,  -720896,  -688128,  -655360,  -622592,
    -589824,  -557056,  -524288,  -491520,  -458752,  -425984,  -393216,  -376832,  -360448,  -344064,  -327680,
    -311296,  -294912,  -278528,  -262144,  -245760,  -229376,  -212992,  -196608,  -180224,  -163840,  -147456,
    -131072,  -114688,  -98304,   -81920,   -65536,   -58982,   -52428,   -45875,   -39321,   -32768,   -26214,
    -19660,   -13107,   -6553,    -3932,    -3276,    -2621,    -1966,    -1310,    -655,     0,        655,
    1310,     1966,     2621,     3276,     3932,     6553,     13107,    19660,    26214,    32768,    39321,
    45875,    52428,    58982,    65536,    81920,    98304,    114688,   131072,   147456,   163840,   180224,
    196608,   212992,   229376,   245760,   262144,   278528,   294912,   311296,   327680,   344064,   360448,
    376832,   393216,   425984,   458752,   491520,   524288,   557056,   589824,   622592,   655360,   688128,
    720896,   753664,   786432,   838860,   891289,   943718,   996147,   1048576,  1101004,  1153433,  1205862,
    1258291,  1310720,  1363148,  1415577,  1468006,  1520435,  1572864};

inline constexpr int32_t kRomPatchSignedCurveQ23[128] = {
    -8388607, -8388607, -8124420, -7864427, -7608627, -7357021, -7109609, -6866390, -6627365, -6392534, -6161897,
    -5935453, -5713203, -5495146, -5281284, -5071615, -4866140, -4664858, -4467770, -4274876, -4086176, -3901669,
    -3721356, -3545237, -3373312, -3205580, -3042042, -2882697, -2727547, -2576590, -2429827, -2287258, -2148882,
    -2014700, -1884712, -1758918, -1637317, -1519910, -1406697, -1297677, -1192852, -1092220, -995782,  -903537,
    -815487,  -731630,  -651967,  -576497,  -505221,  -438139,  -375251,  -316557,  -262056,  -211749,  -165636,
    -123717,  -85991,   -52459,   -29892,   -20758,   -13285,   -7472,    -3321,    -830,     0,        830,
    3321,     7472,     13285,    20758,    29892,    52459,    85991,    123717,   165636,   211749,   262056,
    316557,   375251,   438139,   505221,   576497,   651967,   731630,   815487,   903537,   995782,   1092220,
    1192852,  1297677,  1406697,  1519910,  1637317,  1758918,  1884712,  2014700,  2148882,  2287258,  2429827,
    2576590,  2727547,  2882697,  3042042,  3205580,  3373312,  3545237,  3721356,  3901669,  4086176,  4274876,
    4467770,  4664858,  4866140,  5071615,  5281284,  5495146,  5713203,  5935453,  6161897,  6392534,  6627365,
    6866390,  7109609,  7357021,  7608627,  7864427,  8124420,  8388607};

/**
 * @brief Convierte la intensidad del Virtual Patch (-63..+63) al rango normalizado [-1.0, +1.0]
 *        usando las curvas exactas de la ROM del MS2000 (H8S 0x19B7E..0x19EB0):
 *        - Destinos 0 (Pitch) y 1 (OSC2Pitch): tabla `kRomPatchPitchQ23` (0x2C71C).
 *        - Destino 6 (Pan): lineal (0x2BD1C).
 *        - Resto de destinos (Cutoff, Amp, OSC1Ctrl1, NoiseLevel, LFO2Freq): curva cuadrática `kRomPatchSignedCurveQ23`
 * (0x2C11C).
 */
inline float getHardwarePatchIntensityNorm(int destinationIndex, float signedKnobMinus63To63) noexcept
{
    const float rawIdx = std::clamp(signedKnobMinus63To63 + 64.0f, 1.0f, 127.0f);
    const int i0 = static_cast<int>(rawIdx);
    const int i1 = std::min(127, i0 + 1);
    const float frac = rawIdx - static_cast<float>(i0);

    if (destinationIndex == 0 || destinationIndex == 1)
    {
        const double v0 = static_cast<double>(kRomPatchPitchQ23[i0]);
        const double v1 = static_cast<double>(kRomPatchPitchQ23[i1]);
        const double v = v0 + static_cast<double>(frac) * (v1 - v0);
        return static_cast<float>(std::clamp(v / 1572864.0, -1.0, 1.0));
    }
    if (destinationIndex == 6)
    {
        const double v0 = static_cast<double>(kRomSignedLinearIntensityQ23[i0]);
        const double v1 = static_cast<double>(kRomSignedLinearIntensityQ23[i1]);
        const double v = v0 + static_cast<double>(frac) * (v1 - v0);
        return static_cast<float>(std::clamp(v / 8388607.0, -1.0, 1.0));
    }
    const double v0 = static_cast<double>(kRomPatchSignedCurveQ23[i0]);
    const double v1 = static_cast<double>(kRomPatchSignedCurveQ23[i1]);
    const double v = v0 + static_cast<double>(frac) * (v1 - v0);
    return static_cast<float>(std::clamp(v / 8388607.0, -1.0, 1.0));
}

// ============================================================================
// 8. Tablas de Factor de Pendiente Anti-Aliasing de Osciladores (Y:$0800..$08FF, P:$0646..$0663)
//    Extraídas de `DOCS/DSP56362_XY_Memory_Dump.txt` (líneas 644–675):
//    - `kRomOsc1SlopeQ23` (Y:$0800..$087F, base $0800 en X:$00FA):
//      Factor de pendiente S para el redondeo parabólico de bordes 1 - (1 - u)^2 en OSC1 Saw/Pulse
//      (`P:$0A93..$0AF6`), con u = saturate((1 - |phi|) * S * 256) * 4.
//    - `kRomOsc2SlopeQ23` (Y:$0880..$08FF, base $0881 en X:$0100):
//      Factor de pendiente S para OSC2 Saw/Square/Triangle (`P:$06A2..$0726`).
// ============================================================================
inline constexpr int32_t kRomOsc1SlopeQ23[128] = {
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x733332, 0x606665, 0x499998, 0x38CCCC, 0x28FFFF, 0x207AE0,
    0x18CCCC, 0x13F5C2, 0x11D709, 0x100FFF, 0x098147, 0x090A3D, 0x083333, 0x07F5C2,
    0x07B439, 0x0742B0, 0x070189, 0x0616EF, 0x06F6A4, 0x06A3A0, 0x0597CD, 0x04C16B,
    0x0413C8, 0x0385A8, 0x031035, 0x02AE4E, 0x025C0A, 0x02166A, 0x01DB18, 0x01A83C,
    0x017C63, 0x015660, 0x013544, 0x01184B, 0x00FED4, 0x00E85B, 0x00D474, 0x00C2C2,
    0x00B2F9, 0x00A4DA, 0x0096FE, 0x008136, 0x006E3C, 0x005DBD, 0x004F72, 0x004319,
    0x003876, 0x002F55, 0x002785, 0x0020DD, 0x001B35, 0x001A36, 0x00193B, 0x00183F,
    0x001743, 0x001648, 0x00154C, 0x001450, 0x001355, 0x001259, 0x00115D, 0x001062,
    0x000F66, 0x000E6A, 0x000D6F, 0x000C73, 0x000B78, 0x000A7C, 0x000980, 0x000885,
};

inline constexpr int32_t kRomOsc2SlopeQ23[128] = {
    0x000885, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7FFFFF,
    0x7FFFFF, 0x7FFFFF, 0x7FFFFF, 0x7B1C01, 0x6DAD91, 0x61B645, 0x570D2E, 0x4D8DD6,
    0x4517C2, 0x3D8E00, 0x36D6C8, 0x30DB22, 0x2B8697, 0x26C6EB, 0x228BE1, 0x1EC700,
    0x1B6B64, 0x186D91, 0x15C34B, 0x136375, 0x1145F0, 0x0F6380, 0x0DB5B2, 0x0C36C8,
    0x0AE1A5, 0x09B1BA, 0x08A2F8, 0x07B1C0, 0x06DAD9, 0x061B64, 0x0570D2, 0x04D8DD,
    0x04517C, 0x03D8E0, 0x036D6C, 0x030DB2, 0x02B869, 0x026C6E, 0x0228BE, 0x01EC70,
    0x01B6B6, 0x0186D9, 0x015C34, 0x013637, 0x01145F, 0x00F638, 0x00DB5B, 0x00C36C,
    0x00AE1A, 0x009B1B, 0x008A2F, 0x007B1C, 0x006DAD, 0x0061B6, 0x00570D, 0x004D8D,
    0x004517, 0x003D8E, 0x0036D6, 0x0030DB, 0x002B86, 0x0026C6, 0x00228B, 0x001EC7,
    0x001B6B, 0x00186D, 0x0015C3, 0x001363, 0x001145, 0x000F63, 0x000DB5, 0x000C36,
    0x000AE1, 0x0009B1, 0x0008A2, 0x0007B1, 0x0006DA, 0x00061B, 0x000570, 0x0004D8,
};

/**
 * @brief Devuelve el factor de pendiente anti-aliasing S (`X:$00DB` para OSC1, `X:$00DD` para OSC2)
 *        interpolando linealmente `Y:$0800..$087F` (`isOsc2 = false`) o `Y:$0881..$08FF` (`isOsc2 = true`)
 *        según `P:$0646..$0659` para la frecuencia `freqHz` a la frecuencia de muestreo `sampleRate`.
 */
inline float getHardwareOscSlopeFactor(float freqHz, bool isOsc2 = false,
                                       double sampleRate = kHardwareSampleRate) noexcept
{
    const double sr = std::max(1000.0, sampleRate);
    // Normalizamos a la frecuencia equivalente a 48 kHz (la tabla depende de f0 / fs)
    const double equivFreq48k = std::max(1.0, static_cast<double>(freqHz) * (static_cast<double>(kHardwareSampleRate) / sr));
    const double midiPitch = 69.0 + 12.0 * std::log2(equivFreq48k / 440.0);
    // En P:$062A, OSC1 carga n5 = $0800 (índice = midiPitch) y OSC2 carga n5 = $0881 (índice = midiPitch + 1 dentro de Y:$0880)
    const double tablePos = isOsc2 ? std::clamp(midiPitch + 1.0, 1.0, 127.0)
                                   : std::clamp(midiPitch, 0.0, 127.0);
    const int i0 = static_cast<int>(tablePos);
    const int i1 = std::min(127, i0 + 1);
    const double frac = tablePos - static_cast<double>(i0);
    const int32_t *table = isOsc2 ? kRomOsc2SlopeQ23 : kRomOsc1SlopeQ23;
    const double s0 = static_cast<double>(table[i0]) / kQ23FractionOne;
    const double s1 = static_cast<double>(table[i1]) / kQ23FractionOne;
    return static_cast<float>(std::clamp(s0 + frac * (s1 - s0), 0.0, 1.0));
}

}  // namespace FirmwareTables

}  // namespace ABDMS2000
