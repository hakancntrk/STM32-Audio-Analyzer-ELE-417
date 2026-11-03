// ST7735 kontrolcüsüne komut, RGB565 piksel ve 5x7 metin gönderir.
#include "St7735Display.hpp"
#include "St7735Font.hpp"

// ST7735S Donanım Pin Bağlantıları: DC=PB0, RST=PB1, CS=PB2
#define DC_PORT  GPIOB
#define DC_PIN   GPIO_PIN_0
#define RST_PORT GPIOB
#define RST_PIN  GPIO_PIN_1
#define CS_PORT  GPIOB
#define CS_PIN   GPIO_PIN_2

St7735Display::St7735Display(SPI_HandleTypeDef* hspi) : hspi_(hspi) {}

void St7735Display::init() {
    // Sıfırlama darbeleri arasında kontrolcünün toparlanması beklenir.
    pinRST(false); HAL_Delay(50);
    pinRST(true);  HAL_Delay(50);

    writeCommand(ST7735_Cmd::SwReset);
    HAL_Delay(150);
    writeCommand(ST7735_Cmd::SleepOut);
    HAL_Delay(150);

    // Normal, boşta ve kısmi gösterim için kare hızı kayıtları.
    writeCommand(ST7735_Cmd::FrmCtrl1);
    { uint8_t d[] = {0x01, 0x2C, 0x2D}; writeData(d, 3); }
    writeCommand(ST7735_Cmd::FrmCtrl2);
    { uint8_t d[] = {0x01, 0x2C, 0x2D}; writeData(d, 3); }
    writeCommand(ST7735_Cmd::FrmCtrl3);
    { uint8_t d[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}; writeData(d, 6); }

    // Kontrolcünün güç kayıtları.
    writeCommand(ST7735_Cmd::InvCtrl);
    writeData8(0x07);
    writeCommand(ST7735_Cmd::PwrCtrl1);
    { uint8_t d[] = {0xA2, 0x02, 0x84}; writeData(d, 3); }
    writeCommand(ST7735_Cmd::PwrCtrl2);
    writeData8(0xC5);
    writeCommand(ST7735_Cmd::PwrCtrl3);
    { uint8_t d[] = {0x0A, 0x00}; writeData(d, 2); }
    writeCommand(ST7735_Cmd::VcomCtrl1);
    writeData8(0x0E);
    writeCommand(ST7735_Cmd::InvertOff);

    // MADCTL=0x60, belleği yatay 160x128 düzene çevirir.
    writeCommand(ST7735_Cmd::MemDataCtrl);
    writeData8(0x60);

    // ColMod=0x05, RGB565 piksel biçimini seçer.
    writeCommand(ST7735_Cmd::ColMod);
    writeData8(0x05);

    // Kontrolcünün gama eğrileri.
    writeCommand(ST7735_Cmd::GammaPos);
    {
        uint8_t d[] = {0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
                       0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};
        writeData(d, 16);
    }
    writeCommand(ST7735_Cmd::GammaNeg);
    {
        uint8_t d[] = {0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
                       0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};
        writeData(d, 16);
    }

    writeCommand(ST7735_Cmd::NormalOn);
    HAL_Delay(10);
    writeCommand(ST7735_Cmd::DisplayOn);
    HAL_Delay(100);

    clear(cfg::COL_BG);
}

void St7735Display::clear(uint16_t colour) {
    fillRect(0, 0, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT, colour);
}

void St7735Display::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour) {
    setAddressWindow(x, y, x + w - 1, y + h - 1);

    const uint8_t hi = static_cast<uint8_t>(colour >> 8);
    const uint8_t lo = static_cast<uint8_t>(colour & 0xFF);

    // Küçük yığın tamponuyla 64 piksel gönderilir.
    uint8_t buf[128];
    for (int i = 0; i < 64; ++i) {
        buf[i * 2]     = hi;
        buf[i * 2 + 1] = lo;
    }

    uint32_t totalPixels = static_cast<uint32_t>(w) * h;
    pinDC(true);
    pinCS(false);

    while (totalPixels > 0) {
        const uint16_t chunk = (totalPixels > 64) ? 64 : totalPixels;
        HAL_SPI_Transmit(hspi_, buf, chunk * 2, HAL_MAX_DELAY);
        totalPixels -= chunk;
    }
    pinCS(true);
}

void St7735Display::drawText(uint16_t x, uint16_t y, const char* text, uint16_t colour) {
    while (*text) {
        drawChar(x, y, *text, colour);
        x += 6; // 5 piksel genişlik + 1 piksel karakter arası boşluk
        ++text;
    }
}

void St7735Display::drawChar(uint16_t x, uint16_t y, char c, uint16_t colour) {
    const uint8_t* glyph = Font::glyphFor(c);

    uint8_t buf[5 * 7 * 2];
    const uint8_t hi   = static_cast<uint8_t>(colour >> 8);
    const uint8_t lo   = static_cast<uint8_t>(colour & 0xFF);
    const uint8_t bgHi = static_cast<uint8_t>(cfg::COL_BG >> 8);
    const uint8_t bgLo = static_cast<uint8_t>(cfg::COL_BG & 0xFF);

    // Yazı tablosunda her bayt dikey sütundur: bit 0 üst, bit 6 alt pikseldir.
    for (uint8_t row = 0; row < 7; ++row) {
        for (uint8_t col = 0; col < 5; ++col) {

            bool pixelOn = (glyph[col] & (1 << row)) != 0;
            uint8_t index = (row * 5 + col) * 2;

            if (pixelOn) {
                buf[index]     = hi;
                buf[index + 1] = lo;
            } else {
                buf[index]     = bgHi;
                buf[index + 1] = bgLo;
            }
        }
    }
    setAddressWindow(x, y, x + 4, y + 6);
    pinDC(true);
    pinCS(false);
    HAL_SPI_Transmit(hspi_, buf, sizeof(buf), HAL_MAX_DELAY);
    pinCS(true);
}

void St7735Display::drawTextScaled(uint16_t x, uint16_t y, const char* text, uint16_t colour, uint8_t scale) {
    uint16_t row[cfg::DISPLAY_WIDTH];

    while (*text) {
        const uint8_t* glyph = Font::glyphFor(*text++);

        uint16_t width = 6u * scale;

        for (uint16_t dy = 0; dy < 7u * scale; ++dy) {
            for (uint16_t dx = 0; dx < width; ++dx) {
                const uint16_t col = dx / scale;
                const bool pixelOn = (col < 5u) && (glyph[col] & (1u << (dy / scale)));
                row[dx] = pixelOn ? colour : cfg::COL_BG;
            }
            writePixels(x, y + dy, row, width);
        }
        x += 6u * scale;
    }
}

void St7735Display::writePixels(uint16_t x, uint16_t y, const uint16_t* pixels, uint16_t count) {
    setAddressWindow(x, y, x + count - 1u, y);

    uint8_t buf[128];
    pinDC(true);
    pinCS(false);

    while (count > 0) {
        const uint16_t chunk = count > 64u ? 64u : count;
        for (uint16_t i = 0; i < chunk; ++i) {
            buf[2u * i]      = static_cast<uint8_t>(pixels[i] >> 8);
            buf[2u * i + 1u] = static_cast<uint8_t>(pixels[i] & 0xFF);
        }
        HAL_SPI_Transmit(hspi_, buf, chunk * 2u, HAL_MAX_DELAY);
        pixels += chunk;
        count  -= chunk;
    }
    pinCS(true);
}

void St7735Display::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    writeCommand(ST7735_Cmd::ColAddrSet);
    writeData16(x0);
    writeData16(x1);

    writeCommand(ST7735_Cmd::RowAddrSet);
    writeData16(y0);
    writeData16(y1);

    writeCommand(ST7735_Cmd::MemWrite);
}

void St7735Display::writeCommand(uint8_t cmd) {
    pinDC(false);
    pinCS(false);
    HAL_SPI_Transmit(hspi_, &cmd, 1, HAL_MAX_DELAY);
    pinCS(true);
}

void St7735Display::writeData(const uint8_t* data, uint16_t len) {
    pinDC(true);
    pinCS(false);
    HAL_SPI_Transmit(hspi_, const_cast<uint8_t*>(data), len, HAL_MAX_DELAY);
    pinCS(true);
}

void St7735Display::writeData8(uint8_t data) {
    writeData(&data, 1);
}

void St7735Display::writeData16(uint16_t data) {
    uint8_t buf[2] = {static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)};
    writeData(buf, 2);
}

void St7735Display::pinDC(bool level) {
    HAL_GPIO_WritePin(DC_PORT, DC_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void St7735Display::pinCS(bool level) {
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void St7735Display::pinRST(bool level) {
    HAL_GPIO_WritePin(RST_PORT, RST_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
