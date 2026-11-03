// ADC verisini DMA'nın dairesel çift tamponundan ana döngüye aktarır.
#include "AudioSampler.hpp"
#include "main.h"
#include <cstring>

AudioSampler::AudioSampler(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim)
    : hadc_(hadc), htim_(htim), dmaBuffer_{}, produced_(0), consumed_(0), dropped_(0) {}

void AudioSampler::start() {
    produced_ = 0;
    consumed_ = 0;
    dropped_  = 0;

    if (HAL_ADC_Start_DMA(hadc_, reinterpret_cast<uint32_t*>(dmaBuffer_), 
                          2u * cfg::FFT_SIZE) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_TIM_Base_Start(htim_) != HAL_OK) {
        HAL_ADC_Stop_DMA(hadc_);
        Error_Handler();
    }
}

bool AudioSampler::readFrame(uint16_t *buf) {
    // Çağıranın kesme durumu korunur.
    const uint32_t mask = __get_PRIMASK();
    // Sıra numarası ve DMA sayacı birlikte okunur.
    __disable_irq();
    const uint32_t seq = produced_;
    const uint32_t remaining = __HAL_DMA_GET_COUNTER(hadc_->DMA_Handle);
    __set_PRIMASK(mask);

    // Aynı blok iki kez okunmaz.
    if (seq == consumed_) {
        return false;
    }

    // Tek sıra numarası yarı 0'ı, çift sıra numarası yarı 1'i seçer.
    const uint32_t half = (seq & 1u) != 0 ? 0u : 1u;

    // NDTR=0 veya >2048 ise DMA yarı 0'a yazar; etkin yarı kopyalanmaz.
    const uint32_t active = (remaining == 0 || remaining > cfg::FFT_SIZE) ? 0u : 1u;
    if (half == active) {
        return false;
    }

    memcpy(buf, &dmaBuffer_[half * cfg::FFT_SIZE], cfg::FFT_SIZE * sizeof(uint16_t));

    // Son DMA denetimi kopyalamadan sonra yapılır.
    __DMB();

    // Son denetim ve tüketim kaydı aynı kesmesiz bölgede kalır.
    __disable_irq();
    const uint32_t endRemaining = __HAL_DMA_GET_COUNTER(hadc_->DMA_Handle);
    const uint32_t activeEnd = (endRemaining == 0 || endRemaining > cfg::FFT_SIZE) ? 0u : 1u;

    // Kopyalanırken değişen veya yazılmaya başlanan yarı reddedilir.
    const bool valid = (seq == produced_) && (half != activeEnd);
    if (valid) {
        // Son blok dışındaki sıra farkı kaçan blok sayısına eklenir.
        dropped_ += (seq - consumed_ - 1u);
        consumed_ = seq;
    }
    __set_PRIMASK(mask);

    return valid;
}

void AudioSampler::discard() {
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    consumed_ = produced_;
    __set_PRIMASK(mask);
}

void AudioSampler::onTransferComplete(uint32_t half) {
    uint32_t sequence = produced_ + 1u;
    const uint32_t expectedHalf = (sequence & 1u) != 0 ? 0u : 1u;
    // Atlanan bildirimde sıra sayacı DMA yarısıyla eşleşir.
    if (expectedHalf != half) sequence++;
    produced_ = sequence;
}
