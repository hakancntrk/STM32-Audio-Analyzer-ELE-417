// Ses işleme, ekran yerleşimi ve zamanlama sabitlerini bir arada tutar.
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>

enum class DisplayMode : uint8_t { Spectrum, Waterfall, Waveform, Tuner };

namespace cfg {

// Ses ve analog-sayısal çevirici (ADC) ayarları.
constexpr uint32_t SAMPLE_RATE_HZ      = 96000u;
constexpr uint32_t ADC_RESOLUTION_BITS = 12u;
constexpr float    ADC_MAX_VALUE       = static_cast<float>((1u << ADC_RESOLUTION_BITS) - 1u);

// Hızlı Fourier dönüşümü (FFT) ve tam ölçeğe göre genlik (dBFS) ayarları.
constexpr uint32_t FFT_SIZE            = 2048u;
constexpr uint32_t HALF_FFT            = FFT_SIZE / 2u;
constexpr float    FREQ_BIN_WIDTH_HZ   = static_cast<float>(SAMPLE_RATE_HZ) / FFT_SIZE;

constexpr float    DISPLAY_MIN_HZ      = 50.0f;
constexpr float    DISPLAY_MAX_HZ      = 16000.0f;
constexpr float    DISPLAY_DB_MIN      = -65.0f; // Deneme tabanı; karttaki gürültüye göre ayarlanır.
constexpr float    DISPLAY_DB_MAX      = 0.0f;
constexpr float    SPECTRUM_SMOOTH_ALPHA = 0.4f; // Gösterimde %60 eski, %40 yeni değer.
constexpr float    MIN_AMPLITUDE_FLOOR = 1e-6f; // log10(0) yerine -120 dBFS tabanı
constexpr float    FFT_WINDOW_GAIN     = 4.0f; // Tek taraflı spektrum ve Hann telafisi

// McLeod perde yöntemi (MPM) ve normalize kare fark fonksiyonu (NSDF).
constexpr uint32_t MPM_RATE_HZ         = SAMPLE_RATE_HZ / 2u; // 2 kat alt örnekleme ile 48 kHz
constexpr uint32_t MPM_SIZE            = FFT_SIZE / 2u;
constexpr float    F0_MIN_HZ           = 65.0f;   // En düşük nota frekansı (C2)
constexpr float    F0_MAX_HZ           = 8000.0f; // Tek ton kestiriminin üst sınırı
constexpr float    F0_BOUNDARY_TOL_CENTS = 5.0f; // Sınırdaki küçük kestirim sapmalarını kabul et
constexpr uint32_t MPM_MAX_LAG         = static_cast<uint32_t>(MPM_RATE_HZ / F0_MIN_HZ) + 2u;

constexpr float    MPM_MIN_RMS         = 0.003f;  // Sessizlik veya gürültü için alt RMS eşiği
constexpr float    MPM_MIN_CLARITY     = 0.75f;   // Geçerli nota için en düşük NSDF netlik eşiği
constexpr float    MPM_PEAK_RATIO      = 0.90f;   // İlk güçlü tepeyi seçme oranı
constexpr int      NOTE_NAME_CAPACITY  = 8;       // Sonlandırıcı dahil karakter sayısı
constexpr float    REFERENCE_A4_HZ     = 440.0f;
constexpr float    REFERENCE_A4_MIDI   = 69.0f;   // MIDI: sayısal müzik nota numarası

// Ekran yerleşimi; koordinatlar piksel, sağ ve alt sınırlar dahildir.
// x=0                            x=159
// +---------------------------------+ y=0
// | Mod adı                 LIVE     | y=2..8
// +---------------------------------+ y=11 (ayırıcı)
// | Spektrum / waterfall / dalga     | y=12..91 (80 satır)
// | veya akort göstergesi            |
// +---------------------------------+ y=92 (ayırıcı)
// | Bilgi 0                         | y=94..100
// | Bilgi 1                         | y=104..110
// | Bilgi 2                         | y=114..120
// +---------------------------------+ y=127
constexpr uint16_t DISPLAY_WIDTH       = 160u;
constexpr uint16_t DISPLAY_HEIGHT      = 128u;

constexpr uint16_t SPEC_AREA_X         = 0u;
constexpr uint16_t SPEC_AREA_Y         = 12u;
constexpr uint16_t SPEC_AREA_W         = DISPLAY_WIDTH;
constexpr uint16_t SPEC_AREA_H         = 80u;

constexpr uint16_t INFO_AREA_Y         = 94u;
constexpr uint8_t  INFO_CHARS          = 26u;
constexpr uint32_t INFO_LINE_COUNT     = 3u;
constexpr uint32_t INFO_LINE_SPACING_PX = 10u;
constexpr int      TEXT_LEFT_PX        = 2;
constexpr int      TITLE_TOP_PX        = 2;
constexpr uint32_t STATE_TEXT_WIDTH_PX = 36u;
constexpr int      DISPLAY_MODE_COUNT  = 4;

// Şelale renk eşikleri dBFS; eşitliklerin yönü çizim kodunda korunur.
constexpr float    WATERFALL_BLUE_BELOW_DB   = -60.0f;
constexpr float    WATERFALL_CYAN_BELOW_DB   = -40.0f;
constexpr float    WATERFALL_GREEN_BELOW_DB  = -20.0f;
constexpr float    WATERFALL_YELLOW_BELOW_DB = -8.0f;

constexpr uint32_t WAVE_SIZE           = 1024u;   // Osiloskop penceresindeki örnek sayısı
constexpr float    TUNER_RANGE_CENTS   = 50.0f;   // Akort ibresi aralığı (+/- 50 sent)
constexpr float    TUNER_TOL_CENTS     = 5.0f;    // Akort toleransı (+/- 5 sent)

// RGB565: kırmızı 5 bit, yeşil 6 bit, mavi 5 bit.
constexpr uint16_t COL_BG              = 0x0000;
constexpr uint16_t COL_BAR_LOW         = 0x07E0;
constexpr uint16_t COL_BAR_MID         = 0xFFE0;
constexpr uint16_t COL_BAR_HIGH        = 0xF800;
constexpr uint16_t COL_TEXT            = 0xFFFF;
constexpr uint16_t COL_LABEL           = 0x07FF;
constexpr uint16_t COL_BLUE            = 0x001F;
constexpr uint16_t COL_AXIS            = 0x4208;

// Zamanlama.
constexpr uint32_t BUTTON_DEBOUNCE_MS  = 30u;     // Buton sıçrama filtresi süresi (ms)
constexpr uint32_t DISPLAY_INTERVAL_MS = 100u;    // Ekran veri yenileme aralığı (ms)
constexpr uint32_t PITCH_INTERVAL_MS   = 100u;    // MPM hesaplama aralığı (ms)
constexpr uint16_t DISPLAY_ROWS_PER_STEP = 4u;   // Bir adımda çizilecek satır sayısı
constexpr uint32_t LOG_INTERVAL_MS     = 1000u;   // UART durum raporlama aralığı (ms)

} // namespace cfg

#endif // CONFIG_HPP
