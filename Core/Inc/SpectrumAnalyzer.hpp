// Hann pencereli hızlı Fourier dönüşümünün (FFT) durumunu ve sonuçlarını tutar.
#ifndef SPECTRUM_ANALYZER_HPP
#define SPECTRUM_ANALYZER_HPP

#define __FPU_PRESENT 1U
#include "Config.hpp"
#include "arm_math.h"
#include "main.h"
#include <cstdint>

// Görüntülenen bandın tepe ve ortalama genlik seviyeleri.
struct SpectrumLevels {
    float peakHz;     // En güçlü tepe frekansı (Hz)
    float peakDb;     // Tepe frekansın genliği (dBFS)
    float averageDb;  // Görüntülenen bandın ortalama genliği (dBFS)
};

class SpectrumAnalyzer {
public:
    // Hann katsayılarını ve gerçek FFT çalışma durumunu hazırlar.
    SpectrumAnalyzer();

    // 2048 ADC örneğini alır: DC temizler, Hann penceresi uygular, FFT hesaplar
    void process(const uint16_t *adcSamples);

    // Ekran talep ettiğinde 1024 frekans noktasının dB değerlerini hesaplar
    const float *magnitudeDb() const;

    // Son analiz edilen bloğun özet seviyeleri
    const SpectrumLevels &levels() const { return levels_; }

private:
    // Pozitif sayıda ADC örneğinin ortalamasını ADC biriminde verir; tam sayı toplar.
    static float calculateAdcMean(const uint16_t* samples, uint32_t sampleCount);

    float samples_[cfg::FFT_SIZE];            // DC çıkarılmış ve pencerelenmiş giriş
    float fftBuf_[cfg::FFT_SIZE];             // FFT karmaşık çıkışı ve genlik değerleri
    mutable float magDb_[cfg::HALF_FFT];      // Ekran için önbelleklenmiş dB genlikleri
    mutable bool magnitudeDbPending_ = false; // dB dönüşümü bekliyor bayrağı
    SpectrumLevels levels_{};                 // Özet seviyeler

    // Hızlı CCM-RAM alanında saklanan Hann tablosu
    static float hannWindow_[cfg::FFT_SIZE];
    arm_rfft_fast_instance_f32 fftInstance_;
};

#endif // SPECTRUM_ANALYZER_HPP
