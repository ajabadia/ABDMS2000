#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

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

/**
 * @brief Frecuencia exacta en Hz de LFO1 según la tabla ROM 0x295E8 (DSP56362 P:$0500).
 *        El acumulador de fase de 24 bits con signo recorre 2 * 2^23 muestras por ciclo
 *        a fs = 48.000 kHz: f_Hz = (incQ23 / 2^24) * 48000.0 (0.0458 Hz .. 36.61 Hz).
 */
inline float getHardwareLfo1FreqHz(float midiKnob0to127) noexcept
{
    const float clamped = std::clamp(midiKnob0to127, 0.0f, 127.0f);
    const int i0 = static_cast<int>(clamped);
    const int i1 = std::min(127, i0 + 1);
    const float frac = clamped - static_cast<float>(i0);

    const double inc0 = static_cast<double>(kRomLfo1IncrementQ23[i0]);
    const double inc1 = static_cast<double>(kRomLfo1IncrementQ23[i1]);
    const double inc = inc0 + static_cast<double>(frac) * (inc1 - inc0);
    return static_cast<float>((inc / 16777216.0) * kHardwareSampleRate);
}

/**
 * @brief Frecuencia exacta en Hz de LFO2 según la tabla ROM 0x29BE8 (DSP56362 P:$052A).
 *        Idéntica a LFO1 entre knob 0..114 (0.0458 Hz .. 23.7865 Hz; última
 *        palabra común 0x00207A en 114); difiere primero en 115 y alcanza
 *        194.9043 Hz en 127 (5.3239x LFO1 en ese índice).
 */
inline float getHardwareLfo2FreqHz(float midiKnob0to127) noexcept
{
    const float clamped = std::clamp(midiKnob0to127, 0.0f, 127.0f);
    const int i0 = static_cast<int>(clamped);
    const int i1 = std::min(127, i0 + 1);
    const float frac = clamped - static_cast<float>(i0);

    const double inc0 = static_cast<double>(kRomLfo2IncrementQ23[i0]);
    const double inc1 = static_cast<double>(kRomLfo2IncrementQ23[i1]);
    const double inc = inc0 + static_cast<double>(frac) * (inc1 - inc0);
    return static_cast<float>((inc / 16777216.0) * kHardwareSampleRate);
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
 * `modOctaves` se suma en índices de la tabla (12 índices = 1 octava del word, como EG1/KeyTrack).
 */
inline float getHardwareFilterCutoffHz(float cutoffNorm0to1, float modOctaves = 0.0f) noexcept
{
    const float knob0to127 = std::clamp(cutoffNorm0to1, 0.0f, 1.0f) * 127.0f;
    const float romIndex = std::clamp(128.0f + knob0to127 + (modOctaves * 12.0f), 0.0f, 255.0f);
    const int i0 = static_cast<int>(romIndex);
    const int i1 = std::min(255, i0 + 1);
    const float frac = romIndex - static_cast<float>(i0);

    const double w0 = static_cast<double>(kRomMasterPitchQ23[i0]);
    const double w1 = static_cast<double>(kRomMasterPitchQ23[i1]);
    const double w = (w0 + static_cast<double>(frac) * (w1 - w0)) / kQ23FractionOne;

    const double c = std::min(2.0 * w, kHardwareFilterMaxCoefficient);
    const double hz = (static_cast<double>(kHardwareSampleRate) / 3.14159265358979323846) * std::asin(c);
    return static_cast<float>(std::clamp(hz, 5.0, 20000.0));
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
        return std::clamp(signedKnobMinus63To63 / 63.0f, -1.0f, 1.0f);
    }
    const double v0 = static_cast<double>(kRomPatchSignedCurveQ23[i0]);
    const double v1 = static_cast<double>(kRomPatchSignedCurveQ23[i1]);
    const double v = v0 + static_cast<double>(frac) * (v1 - v0);
    return static_cast<float>(std::clamp(v / 8388607.0, -1.0, 1.0));
}

}  // namespace FirmwareTables

}  // namespace ABDMS2000
