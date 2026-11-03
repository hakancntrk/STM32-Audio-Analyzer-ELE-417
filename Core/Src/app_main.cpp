// ADC örneklerini analiz, ekran ve UART akışına bağlar.
// main.c, uygulamayı configurer() üzerinden başlatır.

// ADC+TIM6 -> DMA çift tampon -> AudioSampler
//                                |-> SpectrumAnalyzer --|
//                                |-> PitchEstimator ----|-> AnalyzerView -> ST7735
#include "main.h"
#include "Config.hpp"
#include "AudioSampler.hpp"
#include "St7735Display.hpp"
#include "SpectrumAnalyzer.hpp"
#include "PitchEstimator.hpp"
#include "AnalyzerView.hpp"
#include "DebouncedButton.hpp"
#include "Telemetry.hpp"

namespace {
constexpr int      STARTUP_TITLE_X_PX   = 38;
constexpr int      STARTUP_TITLE_Y_PX   = 45;
constexpr int      STARTUP_STATE_X_PX   = 47;
constexpr int      STARTUP_STATE_Y_PX   = 65;
constexpr int      STARTUP_DELAY_MS     = 1200;
}

static AudioSampler     *s_sampler = nullptr;

static DisplayMode nextMode(DisplayMode mode);
static void showStartupScreen(St7735Display &display, UART_HandleTypeDef *huart);

extern "C" {

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1 && s_sampler != nullptr) {
        s_sampler->onTransferComplete(0u);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1 && s_sampler != nullptr) {
        s_sampler->onTransferComplete(1u);
    }
}

}

// C bağlantılı configurer adı main.c'deki çağrıyla eşleşir.
extern "C" void configurer(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim,
                           SPI_HandleTypeDef *hspi, UART_HandleTypeDef *huart)
{
    static AudioSampler sampler(hadc, htim);
    static St7735Display display(hspi);
    static SpectrumAnalyzer spectrum;
    static PitchEstimator pitchEst;
    static AnalyzerView view(display);
    static Telemetry telemetry;

    s_sampler = &sampler;

    display.init();
    showStartupScreen(display, huart);

    DebouncedButton modeButton;
    DebouncedButton pauseButton;
    const uint32_t now = HAL_GetTick();
    modeButton.init(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET, now);
    pauseButton.init(HAL_GPIO_ReadPin(PAUSE_GPIO_Port, PAUSE_Pin) == GPIO_PIN_RESET, now);

    if (HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
    sampler.start();

    static uint16_t frame[cfg::FFT_SIZE];
    DisplayMode mode = DisplayMode::Spectrum;
    bool isPaused = false;
    bool skipNextFrame = false;
    bool hasFrameData = false;
    uint32_t frameCount = 0;
    uint32_t lastViewTick = 0;
    uint32_t lastPitchTick = now - cfg::PITCH_INTERVAL_MS;
    PitchResult lastPitch{};

    view.render(mode, isPaused);

    while (true) {
        const uint32_t loopTick = HAL_GetTick();

        // Mod ve duraklatma butonlarını kontrol et.
        if (modeButton.update(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET, loopTick)) {
            mode = nextMode(mode);
            lastPitch = {};
            lastPitchTick = loopTick - cfg::PITCH_INTERVAL_MS;
        }

        if (pauseButton.update(HAL_GPIO_ReadPin(PAUSE_GPIO_Port, PAUSE_Pin) == GPIO_PIN_RESET, loopTick)) {
            isPaused = !isPaused;
            sampler.discard();
            skipNextFrame = !isPaused;
            if (!isPaused) lastPitchTick = loopTick - cfg::PITCH_INTERVAL_MS;
        }

        // Duraklatılmadığında yeni ses karesini analiz et.
        if (isPaused) {
            sampler.discard();
        } else if (sampler.readFrame(frame)) {
            if (skipNextFrame) {
                // Devam edildiğinde ilk kareyi atla.
                skipNextFrame = false;
            } else {
                if (mode != DisplayMode::Tuner) spectrum.process(frame);

                if (mode == DisplayMode::Tuner &&
                    loopTick - lastPitchTick >= cfg::PITCH_INTERVAL_MS) {
                    lastPitch = pitchEst.estimate(frame);
                    lastPitchTick = loopTick;
                }
                frameCount++;

                const bool displayDue = !hasFrameData ||
                    loopTick - lastViewTick >= cfg::DISPLAY_INTERVAL_MS;
                if (displayDue && !view.isRendering()) {
                    const float* magDb = mode == DisplayMode::Tuner ? nullptr : spectrum.magnitudeDb();
                    view.update(magDb, frame, spectrum.levels(), lastPitch);
                    lastViewTick = loopTick;
                    hasFrameData = true;
                }
            }
        }

        view.renderStep(mode, isPaused);

        const Telemetry::Snapshot snapshot{frameCount, mode, isPaused,
            spectrum.levels(), lastPitch, sampler.droppedFrames()};
        telemetry.sendIfDue(huart, snapshot);
    }
}

static void showStartupScreen(St7735Display &display, UART_HandleTypeDef *huart) {
    display.clear(cfg::COL_BG);
    display.drawText(STARTUP_TITLE_X_PX, STARTUP_TITLE_Y_PX, "Audio Analyzer", cfg::COL_LABEL);
    display.drawText(STARTUP_STATE_X_PX, STARTUP_STATE_Y_PX, "Starting...",   cfg::COL_TEXT);

    const uint8_t intro[] = "\r\n=== Audio Analyzer Başlatılıyor ===\r\n";
    HAL_UART_Transmit(huart, intro, sizeof(intro) - 1u, HAL_MAX_DELAY);

    HAL_Delay(STARTUP_DELAY_MS);
    display.clear(cfg::COL_BG);
}

static DisplayMode nextMode(DisplayMode mode) {
    return (mode == DisplayMode::Tuner)
        ? DisplayMode::Spectrum
        : static_cast<DisplayMode>(static_cast<uint8_t>(mode) + 1u);
}
