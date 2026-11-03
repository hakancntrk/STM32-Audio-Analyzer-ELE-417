// ADC bloğundan frekans genliklerini ve özet dBFS seviyelerini hesaplar.
#include "SpectrumAnalyzer.hpp"
#include <cmath>
#include <cstring>

// Hann tablosu hızlı CCM-RAM alanında tutulur, ana SRAM diğer tamponlara kalır.
float SpectrumAnalyzer::hannWindow_[cfg::FFT_SIZE]
    __attribute__((section(".ccmram")));

SpectrumAnalyzer::SpectrumAnalyzer() {
    for (uint32_t n = 0; n < cfg::FFT_SIZE; ++n) {
        hannWindow_[n] = 0.5f * (1.0f - cosf(6.28318530718f * static_cast<float>(n) / cfg::FFT_SIZE));
    }

    arm_rfft_fast_init_f32(&fftInstance_, cfg::FFT_SIZE);
    memset(magDb_, 0, sizeof(magDb_));
}

void SpectrumAnalyzer::process(const uint16_t *adc) {
    const uint32_t N = cfg::FFT_SIZE;

    // DC bileşeni pencere uygulanmadan çıkarılır.
    const float dcOffset = calculateAdcMean(adc, N);

    const float invHalfScale = 2.0f / cfg::ADC_MAX_VALUE;
    for (uint32_t i = 0; i < N; ++i) {
        samples_[i] = (static_cast<float>(adc[i]) - dcOffset) * invHalfScale * hannWindow_[i];
    }

    // CMSIS çıkışı: Re0, Re(N/2), Re1, Im1, ...
    arm_rfft_fast_f32(&fftInstance_, samples_, fftBuf_, 0);

    // 4/N, tek taraflı spektrum ve Hann kazancını telafi eder.
    const float linScale = cfg::FFT_WINDOW_GAIN / static_cast<float>(N);

    const uint32_t firstBin = static_cast<uint32_t>(ceilf(cfg::DISPLAY_MIN_HZ / cfg::FREQ_BIN_WIDTH_HZ));
    const uint32_t lastBin  = static_cast<uint32_t>(floorf(cfg::DISPLAY_MAX_HZ / cfg::FREQ_BIN_WIDTH_HZ));

    float totalMag = 0.0f;
    float maxMag = 0.0f;
    uint32_t peakBin = firstBin;

    for (uint32_t k = 0; k < cfg::HALF_FFT; ++k) {
        const float re = fftBuf_[2u * k];
        const float im = fftBuf_[2u * k + 1u];

        const float mag = (k == 0) ? (fabsf(fftBuf_[0]) * 2.0f / static_cast<float>(N))
                                   : (sqrtf(re * re + im * im) * linScale);

        fftBuf_[k] = mag;

        if (k >= firstBin && k <= lastBin) {
            totalMag += mag;
            if (mag > maxMag) {
                maxMag = mag;
                peakBin = k;
            }
        }
    }

    const float avgMag = totalMag / static_cast<float>(lastBin - firstBin + 1u);
    levels_.peakHz = static_cast<float>(peakBin) * cfg::FREQ_BIN_WIDTH_HZ;
    levels_.peakDb = 20.0f * log10f(fmaxf(maxMag, cfg::MIN_AMPLITUDE_FLOOR));
    levels_.averageDb = 20.0f * log10f(fmaxf(avgMag, cfg::MIN_AMPLITUDE_FLOOR));

    magnitudeDbPending_ = true;
}

// Son FFT'nin 1024 genliğini dBFS'ye çevirir; aynı blok için sonucu önbellekten verir.
const float *SpectrumAnalyzer::magnitudeDb() const {
    // Ekranın çizim yapmadığı karelerde logaritma hesabı yapılmaz
    if (magnitudeDbPending_) {
        for (uint32_t k = 0; k < cfg::HALF_FFT; ++k) {
            magDb_[k] = 20.0f * log10f(fmaxf(fftBuf_[k], cfg::MIN_AMPLITUDE_FLOOR));
        }
        magnitudeDbPending_ = false;
    }
    return magDb_;
}

float SpectrumAnalyzer::calculateAdcMean(const uint16_t* samples, uint32_t sampleCount) {
    uint32_t sum = 0;
    for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        sum += samples[sampleIndex];
    }
    return static_cast<float>(sum) / static_cast<float>(sampleCount);
}
