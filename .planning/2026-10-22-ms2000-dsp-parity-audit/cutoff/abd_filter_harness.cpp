// Medici\u00f3n aislada de la curva CUTOFF(knob) del filtro de ABDMS2000.
// Compila solo la cadena MultiModeFilter + la LUT ROM 0x26154 (sin JUCE).
//
//   cutoff_abd_harness.exe <outdir> <reso0to1> <knob> [knob...]
//
// Por cada knob escribe <outdir>/abd_k<knob>_r<reso>.f32 (mono float32, 48 kHz,
// ruido blanco de amplitud baja para medir la respuesta lineal) e imprime el
// fc nominal que entrega la LUT.
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Core/MS2000FirmwareTables.h"
#include "Filters/MultiModeFilter.h"

namespace
{
constexpr double kSr = 48000.0;
constexpr int kSamples = 480000;  // 10 s: promedia bien la varianza a baja frecuencia

// LCG determinista; el mismo ruido en todas las corridas
uint32_t g_state = 0x12345678u;
float nextNoise()
{
    g_state = g_state * 1664525u + 1013904223u;
    const float u = static_cast<float>((g_state >> 8) & 0xFFFFFF) / 16777216.0f;  // [0,1)
    return (u * 2.0f - 1.0f) * 0.05f;
}
}  // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::printf("uso: %s <outdir> <reso0to1> <knob> [knob...]\n", argv[0]);
        return 2;
    }
    const std::string outdir = argv[1];
    const float reso = static_cast<float>(std::atof(argv[2]));
    bool ir_mode = false;
    bool coeff_mode = false;
    bool saw_mode = false;
    float scale = 1.0f;
    float warp = 1.0f;
    int first = 3;
    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--ir") { ir_mode = true; first = i + 1; }
        else if (a == "--coeff") { coeff_mode = true; first = i + 1; }
        else if (a == "--scale" && i + 1 < argc) { scale = static_cast<float>(std::atof(argv[i + 1])); first = i + 2; }
        else if (a == "--saw") { saw_mode = true; first = i + 1; }
        else if (a == "--warp" && i + 1 < argc) { warp = static_cast<float>(std::atof(argv[i + 1])); first = i + 2; }
    }

    for (int a = first; a < argc; ++a)
    {
        const int knob = std::atoi(argv[a]);
        float norm = static_cast<float>(knob) / 127.0f;
        if (warp != 1.0f)
            norm = std::pow(norm, warp);   // warping del knob -> indice ROM
        float fc = ABDMS2000::FirmwareTables::getHardwareFilterCutoffHz(norm, 0.0f);
        if (coeff_mode)
        {
            // hipotesis: el word ROM es el coeficiente g del SVF (Q23 fraccional)
            const int idx = std::min(255, 47 + knob);
            const double g = static_cast<double>(ABDMS2000::FirmwareTables::kRomFilterCutoffQ23[idx])
                             / static_cast<double>(1u << 23);
            fc = static_cast<float>((kSr / 3.14159265358979323846) * std::atan(g));
        }
        fc *= scale;

        ABDMS2000::MultiModeFilter f;
        f.prepare(kSr);
        f.setType(ABDMS2000::FilterType::LPF24);
        f.setResonance(reso);
        f.setCutoff(fc);

        const int n = ir_mode ? 16384 : (saw_mode ? 48000 : kSamples);
        std::vector<float> out(static_cast<size_t>(n));
        g_state = 0x12345678u;
        const double f0 = 65.406391325149658;   // C2, la misma nota que el emu
        const int nh = static_cast<int>(0.5 * kSr / f0);
        for (int i = 0; i < n; ++i)
        {
            float in;
            if (ir_mode)
                in = (i == 0) ? 1e-3f : 0.0f;
            else if (saw_mode)
            {
                double acc = 0.0;
                for (int h = 1; h <= nh; ++h)
                    acc += std::sin(2.0 * 3.14159265358979323846 * f0 * h * i / kSr) / h;
                in = static_cast<float>(0.25 * acc);
            }
            else
                in = nextNoise();
            out[static_cast<size_t>(i)] = f.process(in);
        }

        char path[512];
        std::snprintf(path, sizeof(path), "%s/abd_%s%s%s%03d_r%03d_s%04d_w%03d.f32", outdir.c_str(),
                      ir_mode ? "ir_" : "", coeff_mode ? "coeff_" : "",
                      saw_mode ? "saw_" : "", knob,
                      static_cast<int>(reso * 100.0f + 0.5f),
                      static_cast<int>(scale * 100.0f + 0.5f),
                      static_cast<int>(warp * 100.0f + 0.5f));
        FILE *fp = std::fopen(path, "wb");
        if (!fp)
        {
            std::printf("[err] no puedo escribir %s\n", path);
            return 1;
        }
        std::fwrite(out.data(), sizeof(float), out.size(), fp);
        std::fclose(fp);
        std::printf("knob=%3d reso=%.2f fc_nominal=%8.2f Hz -> %s\n", knob, reso, fc, path);
    }
    return 0;
}
