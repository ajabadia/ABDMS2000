#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

#include <cstdint>

extern "C" {

/**
 * Initializes the synthesis engine with sample rate.
 */
WASM_EXPORT void initEngine(double sampleRate);

/**
 * Renders audio block (Stereo Left / Right).
 */
WASM_EXPORT void processAudio(float* outL, float* outR, int numSamples);

/**
 * Note On event.
 */
WASM_EXPORT void noteOn(int noteNumber, float velocity);

/**
 * Note Off event.
 */
WASM_EXPORT void noteOff(int noteNumber);

/**
 * All Notes Off (Panic).
 */
WASM_EXPORT void allNotesOff();

/**
 * Sets a parameter by its numeric parameter index.
 */
WASM_EXPORT void setParamNormalized(int paramIndex, float normValue);

/**
 * Sets a parameter by its string identifier.
 */
WASM_EXPORT void setParamById(const char* paramId, float rawValue);

/**
 * Retrieves audio telemetry snapshot for oscilloscope and VU meters.
 */
WASM_EXPORT void getAudioSnapshot(float* scopeOut512, float* vuL, float* vuR, int* activeVoices);

/**
 * Loads a program from the embedded factory bank.
 */
WASM_EXPORT void loadProgram(int programIndex);

/**
 * Resets patch to canonical "Init Synth" default.
 */
WASM_EXPORT void initPatch();

/**
 * Generates musical random patch.
 */
WASM_EXPORT void randomizePatch();

}
