// Son analiz ve çalışma durumunu saniyelik UART metnine dönüştürür.
#include "Telemetry.hpp"
#include <cstdio>
#include <cstring>

namespace {
const char *const MODE_NAMES[cfg::DISPLAY_MODE_COUNT] = {"Spectrum", "Waterfall", "Waveform", "Tuner"};
}

void Telemetry::sendIfDue(UART_HandleTypeDef* uart, const Snapshot& snapshot) {
    constexpr int      TELEMETRY_PITCH_BYTES = 32;
    constexpr int      TELEMETRY_SPECTRUM_BYTES = 80;
    const uint32_t now = HAL_GetTick();
    if (now - lastLogTick_ < cfg::LOG_INTERVAL_MS) return;
    if (HAL_UART_GetState(uart) != HAL_UART_STATE_READY) return;

    char spectrumText[TELEMETRY_SPECTRUM_BYTES] = "Pk: --- Avg: ---";
    if (snapshot.mode != DisplayMode::Tuner) {
        snprintf(spectrumText, sizeof(spectrumText), "Pk: %.0fHz (%.1fdB) Avg: %.1fdB",
                 snapshot.levels.peakHz, snapshot.levels.peakDb, snapshot.levels.averageDb);
    }

    char pitchText[TELEMETRY_PITCH_BYTES] = "---";
    if (snapshot.pitch.valid) {
        snprintf(pitchText, sizeof(pitchText), "%.1fHz %s %+.0fc",
                 snapshot.pitch.f0Hz, snapshot.pitch.noteName, snapshot.pitch.centDeviation);
    }

    const uint32_t newDrops = snapshot.droppedFrames - lastLoggedDrops_;
    snprintf(logBuffer_, sizeof(logBuffer_),
             "[%lu] %-9s %s | %s | f0: %s | Missed : %lu (+%lu)\r\n",
             static_cast<unsigned long>(snapshot.frameCount),
             MODE_NAMES[static_cast<uint8_t>(snapshot.mode)],
             snapshot.isPaused ? "PAUSED" : "LIVE  ",
             spectrumText, pitchText,
             static_cast<unsigned long>(snapshot.droppedFrames),
             static_cast<unsigned long>(newDrops));

    lastLogTick_ = now;
    lastLoggedDrops_ = snapshot.droppedFrames;
    HAL_UART_Transmit_IT(uart, reinterpret_cast<uint8_t*>(logBuffer_), strlen(logBuffer_));
}
