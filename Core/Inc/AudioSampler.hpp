// ADC1 ve TIM6 ile alınan sesi dairesel DMA çift tamponunda tutar.
#ifndef AUDIO_SAMPLER_HPP
#define AUDIO_SAMPLER_HPP

#include "Config.hpp"
#include "stm32f3xx_hal.h"

class AudioSampler {
public:
    // ADC ve zamanlayıcı tanıtıcılarını saklar; blok sayaçlarını sıfırlar.
    AudioSampler(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim);

    // ADC DMA ve TIM6'yı başlatır; HAL hatasında Error_Handler() çağırır.
    void start();

    // Tamamlanan 2048 örnekli ses bloğunu kopyalar.
    // Yeni blok yoksa veya DMA o yarıya yazıyorsa false döner.
    bool readFrame(uint16_t *buf);

    // Duraklatma sonrası bekleyen eski ses bloklarını atar
    void discard();

    // Okunmadan atlanan blok sayısı
    uint32_t droppedFrames() const { return dropped_; }

    // DMA yarım transferinde half=0, tam transferinde half=1
    void onTransferComplete(uint32_t half);

private:
    ADC_HandleTypeDef *hadc_;
    TIM_HandleTypeDef *htim_;

    // DMA bir yarıyı doldururken işlemci diğer 2048 örneği okur.
    uint16_t dmaBuffer_[2u * cfg::FFT_SIZE];

    volatile uint32_t produced_; // Üretilen blok sıra numarası
    uint32_t consumed_;          // Tüketilen blok sıra numarası
    uint32_t dropped_;           // Okunmadan atlanan blok sayısı
};

#endif // AUDIO_SAMPLER_HPP
