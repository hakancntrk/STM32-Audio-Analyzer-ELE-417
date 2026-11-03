// Analiz verilerini ekran sütunlarına ve zaman paylaşımlı çizim adımlarına çevirir.
#include "AnalyzerView.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
const char *const MODE_NAMES[cfg::DISPLAY_MODE_COUNT] = {"Spectrum", "Waterfall", "Waveform", "Tuner"};

// +--------------------------------+ y=12
// |              A4                | y=20..40 (3 kat büyük nota)
// |                                |
// |  -----------|-----------        | y=74 (eksen), ibre y=69..78
// | -50         0         +50       | y=82..88
// +--------------------------------+ y=91
// Geçerli nota: ibre +/-5 sent içinde yeşil, dışında sarı; geçersizse ibre yok.
constexpr uint32_t FONT_ADVANCE_PX     = 6u; // 5 piksel glif + 1 piksel boşluk
constexpr uint32_t FONT_HEIGHT_PX      = 7u;
constexpr int      TUNER_NOTE_SCALE    = 3;
constexpr uint32_t TUNER_NOTE_TOP_PX   = 8u;  // Grafik alanının üstüne göre
constexpr uint32_t TUNER_LEFT_INSET_PX = 8u;
constexpr uint32_t TUNER_RIGHT_INSET_PX = 9u;
constexpr uint32_t TUNER_AXIS_BOTTOM_PX = 18u;
constexpr uint32_t TUNER_TICK_TOP_PX   = 4u;
constexpr int      TUNER_TICK_HEIGHT_PX = 9;
constexpr int      TUNER_NEEDLE_HALF_WIDTH_PX = 1;
constexpr int      TUNER_NEEDLE_WIDTH_PX = 3;
constexpr uint32_t TUNER_NEEDLE_TOP_PX = 5u;
constexpr int      TUNER_NEEDLE_HEIGHT_PX = 10;
constexpr uint32_t TUNER_LABEL_TOP_PX  = 8u;
constexpr uint32_t TUNER_NEG_LABEL_OFFSET_PX = 6u;
constexpr uint32_t TUNER_ZERO_LABEL_OFFSET_PX = 3u;
constexpr uint32_t TUNER_POS_LABEL_OFFSET_PX = 12u;
}

AnalyzerView::AnalyzerView(St7735Display &display) : display_(display) {
    const float ratio = cfg::DISPLAY_MAX_HZ / cfg::DISPLAY_MIN_HZ;

    // Canlı döngüde powf() hesaplamamak için frekans eşleme tablosunu baştan oluştur
    for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
        const float first = cfg::DISPLAY_MIN_HZ *
            powf(ratio, static_cast<float>(x) / cfg::SPEC_AREA_W) / cfg::FREQ_BIN_WIDTH_HZ;
        const float end = cfg::DISPLAY_MIN_HZ *
            powf(ratio, static_cast<float>(x + 1u) / cfg::SPEC_AREA_W) / cfg::FREQ_BIN_WIDTH_HZ;

        firstBin_[x] = static_cast<uint16_t>(first);
        endBin_[x]   = static_cast<uint16_t>(end);
        // Son sütunda üst sınırın hemen altındaki FFT binini de göster.
        if (x + 1u == cfg::SPEC_AREA_W) endBin_[x] = static_cast<uint16_t>(ceilf(end));
        fraction_[x] = first - firstBin_[x];
        bands_[x]    = cfg::DISPLAY_DB_MIN;
    }
}

void AnalyzerView::update(const float *magDb, const uint16_t *adc,
                          const SpectrumLevels &levels, const PitchResult &pitch) {
    // Çizilen kare bitmeden veriyi değiştirmek farklı anları tek ekranda karıştırır.
    if (isRendering()) return;

    if (magDb != nullptr) {
        for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
            const uint16_t first = firstBin_[x];
            const uint16_t end   = endBin_[x];
            float db;

            if (end <= first) {
                // İki bin arasına düşen piksel için doğrusal enterpolasyon
                db = magDb[first] + fraction_[x] * (magDb[first + 1u] - magDb[first]);
            } else {
                // Birden fazla bin kapsayan sütun için en yüksek tepeyi al
                db = cfg::DISPLAY_DB_MIN;
                for (uint16_t bin = first; bin < end; ++bin) {
                    if (magDb[bin] > db) db = magDb[bin];
                }
            }

            db = fmaxf(cfg::DISPLAY_DB_MIN, fminf(db, cfg::DISPLAY_DB_MAX));
            // İlk kare ve tabandaki bantlar yumuşatma beklemeden alınır.
            if (!hasSpectrum_ || db <= cfg::DISPLAY_DB_MIN) {
                bands_[x] = db;
            } else {
                bands_[x] += cfg::SPECTRUM_SMOOTH_ALPHA * (db - bands_[x]);
            }
        }
        hasSpectrum_ = true;
        pushWaterfall();
    } else {
        hasSpectrum_ = false; // Tuner'dan dönüşte eski spektrumla karıştırma.
    }

    levels_ = levels;
    pitch_  = pitch;
    updateWaveform(adc);

    hasFrame_ = true;
    dirty_    = true;
}

void AnalyzerView::pushWaterfall() {
    head_ = (head_ + cfg::SPEC_AREA_H - 1u) % cfg::SPEC_AREA_H;

    for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
        const float db = bands_[x];
        uint8_t idx;
        if (db <= cfg::DISPLAY_DB_MIN) idx = 0;
        else if (db < cfg::WATERFALL_BLUE_BELOW_DB)   idx = 1;
        else if (db < cfg::WATERFALL_CYAN_BELOW_DB)   idx = 2;
        else if (db < cfg::WATERFALL_GREEN_BELOW_DB)  idx = 3;
        else if (db < cfg::WATERFALL_YELLOW_BELOW_DB) idx = 4;
        else                            idx = 5;

        // Bayt [tek x | çift x]: çift sütun baytı başlatır, tek sütun üst yarıyı ekler.
        if ((x & 1u) == 0) {
            history_[head_][x / 2u] = idx;
        } else {
            history_[head_][x / 2u] |= (idx << 4u);
        }
    }
}

void AnalyzerView::updateWaveform(const uint16_t *adc) {
    const float mean = calculateAdcMean(adc, cfg::FFT_SIZE);

    // Dalganın yatay konumu pozitif sıfır geçişiyle sabitlenir.
    uint32_t start = 0;
    for (uint32_t i = 1; i <= cfg::FFT_SIZE - cfg::WAVE_SIZE; ++i) {
        if (static_cast<float>(adc[i - 1u]) < mean && static_cast<float>(adc[i]) >= mean) {
            start = i;
            break;
        }
    }

    const int centre   = cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H / 2u;
    // Önce piksel yarı aralığıyla çarpılır, sonra ADC tam ölçeğine bölünür.
    const float scale  = (cfg::SPEC_AREA_H / 2u - 1u) * 2.0f / cfg::ADC_MAX_VALUE;
    min_ = static_cast<uint16_t>(cfg::ADC_MAX_VALUE);
    max_ = 0;

    for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
        const uint32_t first = start + x * cfg::WAVE_SIZE / cfg::SPEC_AREA_W;
        const uint32_t end   = start + (x + 1u) * cfg::WAVE_SIZE / cfg::SPEC_AREA_W;

        uint16_t lo = adc[first];
        uint16_t hi = lo;

        // Sütuna düşen örneklerin uçları korunur.
        for (uint32_t i = first + 1u; i < end; ++i) {
            if (adc[i] < lo) lo = adc[i];
            if (adc[i] > hi) hi = adc[i];
        }
        if (lo < min_) min_ = lo;
        if (hi > max_) max_ = hi;

        int top    = centre - static_cast<int>((hi - mean) * scale);
        int bottom = centre - static_cast<int>((lo - mean) * scale);

        if (top < cfg::SPEC_AREA_Y) top = cfg::SPEC_AREA_Y;
        if (bottom < cfg::SPEC_AREA_Y) bottom = cfg::SPEC_AREA_Y;
        if (top >= cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H)    top = cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H - 1u;
        if (bottom >= cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H) bottom = cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H - 1u;

        low_[x]  = static_cast<uint16_t>(top);
        high_[x] = static_cast<uint16_t>(bottom);
    }
}

float AnalyzerView::calculateAdcMean(const uint16_t* samples, uint32_t sampleCount) {
    uint32_t sum = 0;
    for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        sum += samples[sampleIndex];
    }
    return static_cast<float>(sum) / static_cast<float>(sampleCount);
}

void AnalyzerView::render(DisplayMode mode, bool paused) {
    do {
        renderStep(mode, paused);
    } while (isRendering() || paused_ != paused);
}

void AnalyzerView::renderStep(DisplayMode mode, bool paused) {
    const bool modeChanged = reset_ || (mode != mode_);

    if (modeChanged) {
        note_[0]     = '\0';
        marker_      = -1;
        mode_        = mode;
        reset_       = false;
        stage_       = Stage::Clear;
        drawRow_     = 0;
        drawPlot_    = true;
        renderPaused_= paused;
        dirty_       = false;
    } else if (!isRendering()) {
        if (!dirty_ && paused == paused_) return;
        stage_       = Stage::Title;
        drawRow_     = 0;
        drawPlot_    = dirty_;
        renderPaused_= paused;
        dirty_       = false;
    }

    switch (stage_) {
    case Stage::Clear: {
        uint16_t count = cfg::DISPLAY_HEIGHT - drawRow_;
        if (count > cfg::DISPLAY_ROWS_PER_STEP) count = cfg::DISPLAY_ROWS_PER_STEP;
        display_.fillRect(0, drawRow_, cfg::DISPLAY_WIDTH, count, cfg::COL_BG);
        drawRow_ += count;
        if (drawRow_ == cfg::DISPLAY_HEIGHT) stage_ = Stage::Axes;
        break;
    }

    case Stage::Axes:
        display_.fillRect(0, cfg::SPEC_AREA_Y - 1u, cfg::DISPLAY_WIDTH, 1, cfg::COL_AXIS);
        display_.fillRect(0, cfg::INFO_AREA_Y - 2u, cfg::DISPLAY_WIDTH, 1, cfg::COL_AXIS);
        stage_ = Stage::Title;
        break;

    case Stage::Title:
        display_.drawText(cfg::TEXT_LEFT_PX, cfg::TITLE_TOP_PX, MODE_NAMES[static_cast<uint8_t>(mode_)], cfg::COL_TEXT);
        stage_ = Stage::State;
        break;

    case Stage::State:
        display_.drawText(cfg::DISPLAY_WIDTH - cfg::STATE_TEXT_WIDTH_PX, cfg::TITLE_TOP_PX,
                          renderPaused_ ? "PAUSED" : "LIVE  ",
                          renderPaused_ ? cfg::COL_BAR_MID : cfg::COL_BAR_LOW);
        drawRow_ = 0;
        stage_   = drawPlot_ ? Stage::Plot : Stage::Idle;
        break;

    case Stage::Plot:
        if (mode_ == DisplayMode::Tuner) {
            drawTunerStep(drawRow_++);
            if (drawRow_ < static_cast<uint16_t>(TunerStep::TotalSteps)) break;
        } else {
            uint16_t end = drawRow_ + cfg::DISPLAY_ROWS_PER_STEP;
            if (end > cfg::SPEC_AREA_H) end = cfg::SPEC_AREA_H;

            switch (mode_) {
            case DisplayMode::Spectrum:  drawSpectrum(drawRow_, end);  break;
            case DisplayMode::Waterfall: drawWaterfall(drawRow_, end); break;
            case DisplayMode::Waveform:  drawWaveform(drawRow_, end);  break;
            case DisplayMode::Tuner:     break;
            }

            drawRow_ = end;
            if (drawRow_ < cfg::SPEC_AREA_H) break;
        }
        infoLine_ = 0;
        stage_    = Stage::Info;
        break;

    case Stage::Info:
        drawInfoLine(infoLine_++);
        if (infoLine_ == cfg::INFO_LINE_COUNT) stage_ = Stage::Idle;
        break;

    case Stage::Idle:
        break;
    }

    if (!isRendering()) paused_ = renderPaused_;
}

void AnalyzerView::drawSpectrum(uint16_t first, uint16_t end) {
    uint16_t heights[cfg::SPEC_AREA_W];

    for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
        const float db = fmaxf(cfg::DISPLAY_DB_MIN, fminf(bands_[x], cfg::DISPLAY_DB_MAX));
        heights[x] = static_cast<uint16_t>((db - cfg::DISPLAY_DB_MIN) *
            cfg::SPEC_AREA_H / (cfg::DISPLAY_DB_MAX - cfg::DISPLAY_DB_MIN));
    }

    for (uint16_t y = first; y < end; ++y) {
        const uint16_t level = cfg::SPEC_AREA_H - y;
        uint16_t colour = cfg::COL_BAR_LOW;
        if (level > cfg::SPEC_AREA_H * 4u / 5u) colour = cfg::COL_BAR_HIGH;
        else if (level > cfg::SPEC_AREA_H / 2u)      colour = cfg::COL_BAR_MID;

        for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
            row_[x] = (heights[x] >= level) ? colour : cfg::COL_BG;
        }
        display_.writePixels(cfg::SPEC_AREA_X, cfg::SPEC_AREA_Y + y, row_, cfg::SPEC_AREA_W);
    }
}

void AnalyzerView::drawWaterfall(uint16_t first, uint16_t end) {
    static const uint16_t colours[] = {
        cfg::COL_BG, cfg::COL_BLUE, cfg::COL_LABEL,
        cfg::COL_BAR_LOW, cfg::COL_BAR_MID, cfg::COL_BAR_HIGH
    };

    for (uint16_t y = first; y < end; ++y) {
        const uint8_t *buf = history_[(head_ + y) % cfg::SPEC_AREA_H];
        for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
            // x=0 -> bayt 0 alt yarı; x=1 -> bayt 0 üst yarı; x=2 -> bayt 1 alt yarı.
            const uint8_t idx = (buf[x / 2u] >> ((x & 1u) * 4u)) & 0x0F;
            row_[x] = colours[idx];
        }
        display_.writePixels(cfg::SPEC_AREA_X, cfg::SPEC_AREA_Y + y, row_, cfg::SPEC_AREA_W);
    }
}

void AnalyzerView::drawWaveform(uint16_t first, uint16_t end) {
    const uint16_t centre = cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H / 2u;

    for (uint16_t y = cfg::SPEC_AREA_Y + first; y < cfg::SPEC_AREA_Y + end; ++y) {
        for (uint16_t x = 0; x < cfg::SPEC_AREA_W; ++x) {
            row_[x] = (y == centre) ? cfg::COL_AXIS : cfg::COL_BG;
            if (y >= low_[x] && y <= high_[x]) {
                row_[x] = cfg::COL_LABEL;
            }
        }
        display_.writePixels(cfg::SPEC_AREA_X, y, row_, cfg::SPEC_AREA_W);
    }
}

void AnalyzerView::drawTunerStep(uint16_t step) {
    const char *note = pitch_.valid ? pitch_.noteName : "---";
    const uint8_t scale = TUNER_NOTE_SCALE;
    const bool noteChanged = (strcmp(note_, note) != 0);

    // Nota aynıysa büyük metni yeniden temizleyip çizmek gerekmez.
    if (step == static_cast<uint16_t>(TunerStep::ClearNoteArea) && noteChanged) {
        display_.fillRect(cfg::SPEC_AREA_X, cfg::SPEC_AREA_Y + TUNER_NOTE_TOP_PX,
                          cfg::SPEC_AREA_W, FONT_HEIGHT_PX * scale, cfg::COL_BG);
    } 
    // Büyük yazı aktarımını tek çağrıya yığmamak için çağrı başına bir karakter.
    else if (step >= static_cast<uint16_t>(TunerStep::DrawNoteCharsStart) &&
             step <= static_cast<uint16_t>(TunerStep::DrawNoteCharsEnd) && noteChanged) {
        const uint16_t index = step - 1u;
        if (index < strlen(note)) {
            const uint16_t x = (cfg::DISPLAY_WIDTH - strlen(note) * FONT_ADVANCE_PX * scale) / 2u;
            const char letter[] = {note[index], '\0'};
            display_.drawTextScaled(x + index * FONT_ADVANCE_PX * scale, cfg::SPEC_AREA_Y + TUNER_NOTE_TOP_PX,
                                    letter, cfg::COL_TEXT, scale);
        }
    }

    const uint16_t left   = cfg::SPEC_AREA_X + TUNER_LEFT_INSET_PX;
    const uint16_t right  = cfg::SPEC_AREA_X + cfg::SPEC_AREA_W - TUNER_RIGHT_INSET_PX;
    const uint16_t centre = (left + right) / 2u;
    const uint16_t y      = cfg::SPEC_AREA_Y + cfg::SPEC_AREA_H - TUNER_AXIS_BOTTOM_PX;

    // Eski ibre silinince altında kalan eksen ve orta çentik yeniden çizilir.
    if (step == static_cast<uint16_t>(TunerStep::DrawScaleAxis)) {
        if (marker_ >= 0) {
            display_.fillRect(marker_ - TUNER_NEEDLE_HALF_WIDTH_PX, y - TUNER_NEEDLE_TOP_PX,
                              TUNER_NEEDLE_WIDTH_PX, TUNER_NEEDLE_HEIGHT_PX, cfg::COL_BG);
        }
        display_.fillRect(left, y, right - left + 1u, 1, cfg::COL_AXIS);
        display_.fillRect(centre, y - TUNER_TICK_TOP_PX, 1, TUNER_TICK_HEIGHT_PX, cfg::COL_TEXT);
        marker_ = -1;
    } 
    // Üç etiket ayrı çağrılarda çizilir; bir adımın ekran aktarımı sınırlı kalır.
    else if (step == static_cast<uint16_t>(TunerStep::DrawLabelNeg50)) {
        display_.drawText(left - TUNER_NEG_LABEL_OFFSET_PX, y + TUNER_LABEL_TOP_PX, "-50", cfg::COL_TEXT);
    } else if (step == static_cast<uint16_t>(TunerStep::DrawLabelZero)) {
        display_.drawText(centre - TUNER_ZERO_LABEL_OFFSET_PX, y + TUNER_LABEL_TOP_PX, "0", cfg::COL_TEXT);
    } else if (step == static_cast<uint16_t>(TunerStep::DrawLabelPos50)) {
        display_.drawText(right - TUNER_POS_LABEL_OFFSET_PX, y + TUNER_LABEL_TOP_PX, "+50", cfg::COL_TEXT);
    } 
    // Nota metni tamamlanınca önbelleğe alınır; geçersiz perdede yeni ibre çizilmez.
    else if (step == static_cast<uint16_t>(TunerStep::DrawNeedle)) {
        strcpy(note_, note);
        if (!pitch_.valid) return;

        const float cents = fmaxf(-cfg::TUNER_RANGE_CENTS,
                                  fminf(pitch_.centDeviation, cfg::TUNER_RANGE_CENTS));
        marker_ = left + static_cast<int>((cents + cfg::TUNER_RANGE_CENTS) *
                  (right - left) / (2.0f * cfg::TUNER_RANGE_CENTS));

        const uint16_t colour = (fabsf(cents) <= cfg::TUNER_TOL_CENTS)
                                    ? cfg::COL_BAR_LOW : cfg::COL_BAR_MID;
        display_.fillRect(marker_ - TUNER_NEEDLE_HALF_WIDTH_PX, y - TUNER_NEEDLE_TOP_PX,
                          TUNER_NEEDLE_WIDTH_PX, TUNER_NEEDLE_HEIGHT_PX, colour);
    }
}

void AnalyzerView::drawInfoLine(uint8_t index) {
    char text[cfg::INFO_LINE_COUNT][cfg::INFO_CHARS + 1u] = {};

    if (mode_ == DisplayMode::Spectrum || mode_ == DisplayMode::Waterfall) {
        snprintf(text[0], sizeof(text[0]), "50Hz       1k          16k");
        if (hasFrame_) {
            snprintf(text[1], sizeof(text[1]), "Pk:%.0fHz %.0fdBFS", levels_.peakHz, levels_.peakDb);
            snprintf(text[2], sizeof(text[2]), "Avg: %.0f dBFS", levels_.averageDb);
        } else {
            snprintf(text[1], sizeof(text[1]), "Pk: ---");
            snprintf(text[2], sizeof(text[2]), "Avg: ---");
        }
    } else if (mode_ == DisplayMode::Waveform) {
        snprintf(text[0], sizeof(text[0]), "Window: %.1fms",
                 cfg::WAVE_SIZE * 1000.0f / cfg::SAMPLE_RATE_HZ);
        snprintf(text[1], sizeof(text[1]), "ADC: %u - %u", min_, max_);
        snprintf(text[2], sizeof(text[2]), "Fs: %.0fkHz", cfg::SAMPLE_RATE_HZ / 1000.0f);
    } else {
        if (pitch_.valid) {
            snprintf(text[0], sizeof(text[0]), "f0:%.1fHz Cents:%+.1f",
                     pitch_.f0Hz, pitch_.centDeviation);
            if (pitch_.centDeviation < -cfg::TUNER_TOL_CENTS) {
                snprintf(text[1], sizeof(text[1]), "Tune up");
                snprintf(text[2], sizeof(text[2]), "Tighten the string");
            } else if (pitch_.centDeviation > cfg::TUNER_TOL_CENTS) {
                snprintf(text[1], sizeof(text[1]), "Tune down");
                snprintf(text[2], sizeof(text[2]), "Loosen the string");
            } else {
                snprintf(text[1], sizeof(text[1]), "In tune");
            }
        } else {
            snprintf(text[0], sizeof(text[0]), "f0:---Hz Cents:---");
            snprintf(text[1], sizeof(text[1]), "Play one string");
        }
    }

    char line[cfg::INFO_CHARS + 1u];
    snprintf(line, sizeof(line), "%-26.26s", text[index]);
    display_.drawText(cfg::TEXT_LEFT_PX, cfg::INFO_AREA_Y + cfg::INFO_LINE_SPACING_PX * index, line, cfg::COL_TEXT);
}
