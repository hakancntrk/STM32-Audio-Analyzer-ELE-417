// Mekanik butonun kontak sıçramalarını süre filtresiyle süzer.
#ifndef DEBOUNCED_BUTTON_HPP
#define DEBOUNCED_BUTTON_HPP

#include "Config.hpp"
#include <cstdint>

class DebouncedButton {
public:
    // İlk buton durumunu ve zamanını ms cinsinden kaydeder.
    void init(bool isPressed, uint32_t nowMs) {
        lastRead_     = isPressed;
        debounced_    = isPressed;
        lastChangeMs_ = nowMs;
    }

    // Yalnızca yeni ve kararlı bir basış anında bir kez true döner
    bool update(bool isPressed, uint32_t nowMs) {
        if (isPressed != lastRead_) {
            lastRead_     = isPressed;
            lastChangeMs_ = nowMs;
        }

        if (lastRead_ != debounced_ && (nowMs - lastChangeMs_ >= cfg::BUTTON_DEBOUNCE_MS)) {
            debounced_ = lastRead_;
            return debounced_; // Basış anında true, bırakışta false döner
        }

        return false;
    }

private:
    bool     lastRead_     = false;
    bool     debounced_    = false;
    uint32_t lastChangeMs_ = 0;
};

#endif // DEBOUNCED_BUTTON_HPP
