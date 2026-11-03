// ST7735 ekranının komut, piksel ve metin çizim arayüzünü tanımlar.
#ifndef ST7735_DISPLAY_HPP
#define ST7735_DISPLAY_HPP

#include "Config.hpp"
#include "stm32f3xx_hal.h"

// ST7735S Temel Komut Kodları
namespace ST7735_Cmd {
    enum : uint8_t {
    	SwReset = 0x01, SleepOut = 0x11, NormalOn = 0x13, InvertOff = 0x20, DisplayOn = 0x29,
    	ColAddrSet = 0x2A, RowAddrSet = 0x2B, MemWrite = 0x2C, MemDataCtrl = 0x36, ColMod = 0x3A,
    	FrmCtrl1 = 0xB1, FrmCtrl2 = 0xB2, FrmCtrl3 = 0xB3, InvCtrl = 0xB4, PwrCtrl1 = 0xC0,
    	PwrCtrl2 = 0xC1, PwrCtrl3 = 0xC2, VcomCtrl1 = 0xC5, GammaPos = 0xE0, GammaNeg = 0xE1
    	} ;
}

// Çizim çağrıları ekran içinde, sıfırdan büyük boyutlarla yapılır.
class St7735Display {
public:
    explicit St7735Display(SPI_HandleTypeDef* hspi);

    // ST7735S'yi yatay 160x128 RGB565 düzeninde başlatır.
    void init();

    void clear(uint16_t colour = cfg::COL_BG);

    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);

    // Standart 5x7 metin yazar (x, y sol-üst köşe)
    void drawText(uint16_t x, uint16_t y, const char* text, uint16_t colour);

    // Metni tam sayı ölçeğiyle büyütür.
    void drawTextScaled(uint16_t x, uint16_t y, const char* text, uint16_t colour, uint8_t scale);

    // Bir satır RGB565 pikseli üst bayt önce olacak şekilde gönderir.
    void writePixels(uint16_t x, uint16_t y, const uint16_t* pixels, uint16_t count);

private:
    void drawChar(uint16_t x, uint16_t y, char c, uint16_t colour);
    void writeCommand(uint8_t cmd);
    // Kontrolcüye len bayt veriyi kesintisiz bir seçim aralığında gönderir.
    void writeData(const uint8_t* data, uint16_t len);
    void writeData8(uint8_t data);
    // 16-bit veriyi önce üst bayt olacak şekilde gönderir.
    void writeData16(uint16_t data);
    // Son koordinatlar dahil olacak şekilde piksel yazma penceresini seçer.
    void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    // Donanım pin kontrolleri (PB0: DC, PB1: RST, PB2: CS)
    void pinDC(bool level);
    void pinCS(bool level);
    void pinRST(bool level);

    SPI_HandleTypeDef* hspi_;
};

#endif // ST7735_DISPLAY_HPP
