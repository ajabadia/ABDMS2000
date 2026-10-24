#pragma once

namespace ABDMS2000
{
namespace Tests
{

/**
 * @brief Ejecuta la batería completa de sondas de calibración DSP de Nivel 2 (por bloque)
 *        utilizando los estimadores metrológicos portados de ABDAudioLab (OLS R^2, Goertzel/DFT,
 *        seguidor de envolvente hilbert/rectificado, detector de ecos e integral de Schroeder,
 *        autocorrelación de moduladores cíclicos y cruces por cero sub-muestra) frente a las
 *        tablas de referencia del firmware Korg MS2000 v1.07 (x811v107.sys / DSP56362).
 */
void testTier2FirmwareBlockCalibration();

}  // namespace Tests
}  // namespace ABDMS2000
