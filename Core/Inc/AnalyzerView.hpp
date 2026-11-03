// Dört analiz modunu küçük çizim adımlarıyla ST7735 ekranına aktarır.
#ifndef ANALYZER_VIEW_HPP
#define ANALYZER_VIEW_HPP

#include "Config.hpp"
#include "PitchEstimator.hpp"
#include "SpectrumAnalyzer.hpp"
#include "St7735Display.hpp"

class AnalyzerView {
public:
    // Ekran tanıtıcısını ve logaritmik frekans eşleme önbelleğini hazırlar.
    explicit AnalyzerView(St7735Display &display);

    // dBFS bantlarını yumuşatır; aynı bantları Spectrum ve Waterfall için saklar.
    // Yeni analiz verileri ekran çizimi bitene kadar sabit kalır.
    // magDb=nullptr ise spektrum ve waterfall geçmişi güncellenmez.
    void update(const float *magDb, const uint16_t *adc,
                const SpectrumLevels &levels, const PitchResult &pitch);

    // Açılışta bütün çizim adımlarını tamamlar.
    void render(DisplayMode mode, bool paused);

    // Her ana döngü turunda bir aşamayı, grafik alanında en çok dört satırı çizer.
    void renderStep(DisplayMode mode, bool paused);

    bool isRendering() const { return stage_ != Stage::Idle; }

private:
    // Mod değişimi: Clear(32) -> Axes(1) -> Title(1) -> State(1) -> Plot -> Info(3) -> Idle.
    // Parantezler çağrı sayısıdır; Plot grafiklerde 20, tuner'da 14 çağrı sürer.
    // Veri yenileme: Idle -> Title -> State -> Plot -> Info -> Idle.
    // Yeni veri yokken yalnız pause değişirse: Idle -> Title -> State -> Idle.
    enum class Stage : uint8_t { Idle, Clear, Axes, Title, State, Plot, Info };

    // Tuner: ClearNoteArea(1) -> DrawNoteChars(8) -> DrawScaleAxis(1)
    //       -> DrawLabelNeg50(1) -> DrawLabelZero(1) -> DrawLabelPos50(1)
    //       -> DrawNeedle(1); toplam 14 çağrı, nota aynıysa ilk dokuzu çizmez.
    enum class TunerStep : uint8_t {
        ClearNoteArea = 0,
        DrawNoteCharsStart = 1,
        DrawNoteCharsEnd = 8,
        DrawScaleAxis = 9,
        DrawLabelNeg50 = 10,
        DrawLabelZero = 11,
        DrawLabelPos50 = 12,
        DrawNeedle = 13,
        TotalSteps = 14
    };

    // Son dBFS bantlarını 4-bit renk indislerine çevirip geçmişe ekler.
    void pushWaterfall();
    // ADC bloğundan tetiklenmiş dalganın sütun başına piksel sınırlarını çıkarır.
    void updateWaveform(const uint16_t *adc);
    // Pozitif sayıda ADC örneğinin ortalamasını ADC biriminde verir; tam sayı toplar.
    static float calculateAdcMean(const uint16_t* samples, uint32_t sampleCount);

    // [first,end) satır aralığında spektrum sütunlarını çizer; birim pikseldir.
    void drawSpectrum(uint16_t first, uint16_t end);
    // Geçmişin [first,end) satırlarını en yeni satır üstte olacak şekilde çizer.
    void drawWaterfall(uint16_t first, uint16_t end);
    // [first,end) satırlarında hazırlanmış dalga sınırlarını çizer.
    void drawWaveform(uint16_t first, uint16_t end);
    // Akort ekranının 14 alt adımından birini çizer; step çağrı indisidir.
    void drawTunerStep(uint16_t step);
    // İstenen bilgi satırını, 26 karakterlik sabit genişlikte çizer.
    void drawInfoLine(uint8_t index);

    St7735Display &display_;

    // Sütun başına FFT indisleri ve kesir payı; canlı döngüde powf gerektirmez.
    uint16_t firstBin_[cfg::SPEC_AREA_W];
    uint16_t endBin_[cfg::SPEC_AREA_W];
    float fraction_[cfg::SPEC_AREA_W];
    float bands_[cfg::SPEC_AREA_W];

    // 6400 baytlık dairesel geçmiş; head_ en yeni satırın indisidir.
    // Bir bayt: [tek x: üst 4 bit | çift x: alt 4 bit], renk indisi 0..5.
    // x:         0  1  2  3 ...     satır: (head_ + y) % 80
    // bayt:      0  0  1  1 ...     head_ her kayıtta bir satır geri gider.
    uint8_t history_[cfg::SPEC_AREA_H][cfg::SPEC_AREA_W / 2u] = {};
    uint16_t head_ = 0;

    // Osiloskop modu min/max piksel sınırları
    uint16_t low_[cfg::SPEC_AREA_W]  = {};
    uint16_t high_[cfg::SPEC_AREA_W] = {};
    uint16_t min_ = 0;
    uint16_t max_ = 0;

    uint16_t row_[cfg::SPEC_AREA_W];
    char note_[cfg::NOTE_NAME_CAPACITY] = {};
    int marker_   = -1; // Akort ibresinin ekrandaki son X koordinatı

    SpectrumLevels levels_{};
    PitchResult pitch_{};
    DisplayMode mode_ = DisplayMode::Spectrum;

    bool paused_       = false;
    bool dirty_        = true;
    bool reset_        = true;
    bool hasFrame_     = false;
    bool hasSpectrum_  = false; // Yumuşatmada kullanılacak önceki spektrum hazır mı?
    Stage stage_       = Stage::Idle;
    uint16_t drawRow_  = 0;
    uint8_t infoLine_  = 0;
    bool drawPlot_     = false;
    bool renderPaused_ = false;


};

#endif // ANALYZER_VIEW_HPP
