// Son analiz ve çalışma durumunu saniyelik UART metnine dönüştürür.
#ifndef TELEMETRY_HPP
#define TELEMETRY_HPP

#include "Config.hpp"
#include "SpectrumAnalyzer.hpp"
#include "PitchEstimator.hpp"
#include "stm32f3xx_hal.h"

class Telemetry {
public:
    struct Snapshot {
        uint32_t frameCount;
        DisplayMode mode;
        bool isPaused;
        const SpectrumLevels& levels;
        const PitchResult& pitch;
        uint32_t droppedFrames;
    };

    // En erken 1000 ms'de bir, UART boşsa analiz ve durum satırını gönderir.
    void sendIfDue(UART_HandleTypeDef* uart, const Snapshot& snapshot);

private:
    static constexpr int      TELEMETRY_BUFFER_BYTES = 192;

    uint32_t lastLogTick_ = 0;
    uint32_t lastLoggedDrops_ = 0;
    // Gönderim kesmeyle sürdüğü için metin fonksiyondan sonra da bellekte kalır.
    char logBuffer_[TELEMETRY_BUFFER_BYTES] = {};
};

#endif // TELEMETRY_HPP
