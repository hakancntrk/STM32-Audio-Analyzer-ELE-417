// McLeod perde yönteminin (MPM) çalışma tamponlarını ve nota sonucunu tanımlar.
#ifndef PITCH_ESTIMATOR_HPP
#define PITCH_ESTIMATOR_HPP

#include "Config.hpp"
#include <cstdint>

// Temel frekans ve nota analiz sonucu
struct PitchResult {
    float f0Hz          = 0.0f;   // Temel frekans (Hz)
    char  noteName[cfg::NOTE_NAME_CAPACITY] = "---"; // En yakın nota (örn: "A4", "C#5")
    float centDeviation = 0.0f;   // Sent sapması (-50.0 ile +50.0 arası)
    bool  valid         = false;  // Geçerli bir nota bulundu mu
};

class PitchEstimator {
public:
    // ADC bloğundan temel frekansı Hz, en yakın notayı ve sent sapmasını bulur.
    PitchResult estimate(const uint16_t* adcSamples);

private:
    // İki kat seyreltir; DC'yi çıkarıp RMS eşiğini denetler.
    bool preprocessSignal(const uint16_t* adc, float& outEnergy);

    // Normalize kare fark fonksiyonunu hesaplar.
    void computeNsdf(float energy);

    // Pozitif lobların tepelerini bulur.
    uint32_t findPositiveLobePeaks(float& outHighestClarity);

    // Üç komşu NSDF değerinden tepenin konumunu ve yüksekliğini bul
    bool interpolatePeak(uint32_t lag, float& outPeriod, float& outHeight) const;

    // İlk güçlü tepeden temel frekansı bulur.
    float refinePitchFrequency(uint32_t count, float highestClarity);

    // Frekanstan nota adı ve sent sapmasını bulur.
    PitchResult frequencyToNote(float frequency);

    float    samples_[cfg::MPM_SIZE]      = {}; // 48 kHz örnek tamponu (1024 eleman)
    float    nsdf_[cfg::MPM_MAX_LAG + 1u] = {}; // NSDF: normalize kare fark fonksiyonu
    uint16_t keyLags_[cfg::MPM_MAX_LAG]   = {}; // Pozitif lob tepe gecikmeleri
};

#endif // PITCH_ESTIMATOR_HPP

