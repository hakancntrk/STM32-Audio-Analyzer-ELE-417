// McLeod perde yöntemiyle (MPM) temel frekansı ve en yakın notayı bulur.
#include "PitchEstimator.hpp"
#include <cmath>
#include <cstdio>

// Yalnızca bu dosyadaki MPM fonksiyonlarını hız için derle.
#pragma GCC optimize ("O3")

PitchResult PitchEstimator::estimate(const uint16_t* adc) {
    PitchResult result;
    float energy = 0.0f;

    if (!preprocessSignal(adc, energy)) {
        return result; // Sinyal çok zayıf (sessizlik veya gürültü tabanı)
    }

    computeNsdf(energy);

    float highestClarity = 0.0f;
    uint32_t peakCount = findPositiveLobePeaks(highestClarity);

    if (highestClarity < cfg::MPM_MIN_CLARITY || peakCount == 0) {
        return result; // Belirgin bir harmonik yapı bulunamadı
    }

    // İlk güçlü tepeyi seçmek, sonraki periyottan yarım frekans üretmeyi önler.
    float frequency = refinePitchFrequency(peakCount, highestClarity);
    if (frequency == 0.0f) {
        return result;
    }

    return frequencyToNote(frequency);
}

bool PitchEstimator::preprocessSignal(const uint16_t* adc, float& outEnergy) {
    const uint32_t size = cfg::MPM_SIZE;

    float mean = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        samples_[i] = 0.5f * (static_cast<float>(adc[2 * i]) + static_cast<float>(adc[2 * i + 1]));
        mean += samples_[i];
    }
    mean /= static_cast<float>(size);

    const float invMax = 2.0f / cfg::ADC_MAX_VALUE;
    float energy = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        samples_[i] = (samples_[i] - mean) * invMax;
        energy += samples_[i] * samples_[i];
    }

    outEnergy = energy;
    const float rms = sqrtf(energy / static_cast<float>(size));
    return (rms >= cfg::MPM_MIN_RMS);
}

// NSDF = 2*r(tau)/m(tau): sinyalin kaydırılmış haliyle benzerliği.
// r: çarpım toplamı; m: çakışan örnek çiftlerinin kare toplamı.
// Pozitif tepenin gecikmesi periyottur: f = örnek hızı / periyot.
// İki gecikme aynı örnekleri paylaşır; dört toplam veri bağımlılığını azaltır.
// CCM ve noinline, hesabı hızlı bellekte tutar.
__attribute__((section(".ccmtext"), noinline))
void PitchEstimator::computeNsdf(float energy) {
    const uint32_t size   = cfg::MPM_SIZE;
    const uint32_t maxLag = cfg::MPM_MAX_LAG;

    // Normalizasyon paydası: m(tau) = sum(x[i]^2 + x[i+tau]^2)
    // Her kaydırmada çakışan pencerenin dışına çıkan uçların kareleri düşülür.
    float norm = 2.0f * energy;

    for (uint32_t lag = 1; lag <= maxLag; lag += 2u) {
        const float left  = samples_[lag - 1u];
        const float right = samples_[size - lag];
        norm -= (left * left + right * right);

        // Aynı dört örnek, lag ve lag+1 kaydırmalarının çarpım toplamına katılır.
        float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
        float next0 = 0.0f, next1 = 0.0f, next2 = 0.0f, next3 = 0.0f;

        const uint32_t count = size - lag - 1u;
        uint32_t i = 0;
        for (; i + 3u < count; i += 4u) {
            const float a0 = samples_[i],      a1 = samples_[i + 1u];
            const float a2 = samples_[i + 2u], a3 = samples_[i + 3u];

            const float b0 = samples_[i + lag],      b1 = samples_[i + lag + 1u];
            const float b2 = samples_[i + lag + 2u], b3 = samples_[i + lag + 3u];
            const float b4 = samples_[i + lag + 4u];

            sum0 += a0 * b0;   next0 += a0 * b1;
            sum1 += a1 * b1;   next1 += a1 * b2;
            sum2 += a2 * b2;   next2 += a2 * b3;
            sum3 += a3 * b3;   next3 += a3 * b4;
        }

        float correlation     = (sum0 + sum1) + (sum2 + sum3);
        float nextCorrelation = (next0 + next1) + (next2 + next3);

        for (; i < count; ++i) {
            correlation     += samples_[i] * samples_[i + lag];
            nextCorrelation += samples_[i] * samples_[i + lag + 1u];
        }
        correlation += samples_[count] * samples_[size - 1u];

        // NSDF = 2 * r(tau) / m(tau)
        nsdf_[lag] = (norm > 0.0f) ? (2.0f * correlation / norm) : 0.0f;

        if (lag < maxLag) {
            const float nextLeft  = samples_[lag];
            const float nextRight = samples_[size - lag - 1u];
            norm -= (nextLeft * nextLeft + nextRight * nextRight);

            nsdf_[lag + 1u] = (norm > 0.0f) ? (2.0f * nextCorrelation / norm) : 0.0f;
        }
    }
}

uint32_t PitchEstimator::findPositiveLobePeaks(float& outHighestClarity) {
    const uint32_t maxLag = cfg::MPM_MAX_LAG;
    uint32_t lag = 1;
    uint32_t count = 0;
    float highest = 0.0f;

    // Sıfır çevresindeki ilk pozitif lob periyot adayı olmadığı için atlanır.
    while (lag <= maxLag && nsdf_[lag] > 0.0f) {
        lag++;
    }

    while (lag <= maxLag) {
        while (lag <= maxLag && nsdf_[lag] <= 0.0f) {
            lag++;
        }

        uint32_t peak = 0;
        while (lag <= maxLag && nsdf_[lag] > 0.0f) {
            if (peak == 0 || nsdf_[lag] > nsdf_[peak]) {
                peak = lag;
            }
            lag++;
        }

        float period = 0.0f;
        float height = 0.0f;
        if (interpolatePeak(peak, period, height)) {
            keyLags_[count++] = static_cast<uint16_t>(peak);
            if (height > highest) {
                highest = height;
            }
        }
    }

    outHighestClarity = highest;
    return count;
}

bool PitchEstimator::interpolatePeak(uint32_t lag, float& outPeriod, float& outHeight) const {
    if (lag <= 1u || lag >= cfg::MPM_MAX_LAG) return false;

    const float left   = nsdf_[lag - 1u];
    const float center = nsdf_[lag];
    const float right  = nsdf_[lag + 1u];
    const float curvature = left - 2.0f * center + right;
    if (center < left || center < right || curvature >= 0.0f) return false;

    // Tepe örnekler arasındaysa parabolün tepe noktasını kullanır.
    const float offset = 0.5f * (left - right) / curvature;
    outPeriod = static_cast<float>(lag) + offset;
    outHeight = center - 0.25f * (left - right) * offset;
    return true;
}

float PitchEstimator::refinePitchFrequency(uint32_t count, float highestClarity) {
    const float threshold = highestClarity * cfg::MPM_PEAK_RATIO;
    const float boundaryRatio = powf(2.0f, cfg::F0_BOUNDARY_TOL_CENTS / 1200.0f);

    for (uint32_t i = 0; i < count; ++i) {
        float period = 0.0f;
        float height = 0.0f;
        if (!interpolatePeak(keyLags_[i], period, height)) {
            continue;
        }
        if (height < threshold || height < cfg::MPM_MIN_CLARITY) {
            continue;
        }

        const float candidateHz = static_cast<float>(cfg::MPM_RATE_HZ) / period;
        if (candidateHz >= cfg::F0_MIN_HZ / boundaryRatio &&
            candidateHz <= cfg::F0_MAX_HZ * boundaryRatio) {
            return candidateHz;
        }

        // Sınır dışındaki temel yerine sonraki periyodu seçip yarım frekans verme
        return 0.0f;
    }

    return 0.0f;
}

PitchResult PitchEstimator::frequencyToNote(float frequency) {
    PitchResult result;
    static const char* const NOTE_NAMES[] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };

    // MIDI = 69 + 12*log2(f/440); her oktav 12 yarım sestir.
    const float midi = cfg::REFERENCE_A4_MIDI + 12.0f *
        log2f(frequency / cfg::REFERENCE_A4_HZ);
    const int nearestMidi = static_cast<int>(roundf(midi));

    const int noteIndex = nearestMidi % 12;
    const int octave    = (nearestMidi / 12) - 1;

    snprintf(result.noteName, sizeof(result.noteName), "%s%d", NOTE_NAMES[noteIndex], octave);
    result.f0Hz          = frequency;
    result.centDeviation = (midi - static_cast<float>(nearestMidi)) * 100.0f;
    result.valid         = true;

    return result;
}
