#ifndef RAW_OLED_DISPLAY_H
#define RAW_OLED_DISPLAY_H

#include "display.h"
#include "ssd1306_spi.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>
#include <cstdint>
#include <vector>

class RawOledDisplay : public Display {
public:
    RawOledDisplay(ssd1306_spi_handle_t oled, int width, int height);
    ~RawOledDisplay();

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ShowBitmap(const char* title, int bitmap_width, int bitmap_height, const std::vector<std::string>& rows, int duration_ms = 15000) override;
    void ClearChatMessages() override;
    void SetPowerSaveMode(bool on) override;

private:
    ssd1306_spi_handle_t oled_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    std::string status_;
    std::string notification_;
    std::string role_;
    std::string message_;
    bool power_save_ = false;
    bool bitmap_overlay_active_ = false;
    int64_t bitmap_until_us_ = 0;
    esp_timer_handle_t bitmap_timer_ = nullptr;

    bool Lock(int timeout_ms = 0) override;
    void Unlock() override;

    void Render();
    void DrawTextLine(int x, int y, const std::string& text, int scale = 1);
    std::string Sanitize(const char* text, size_t max_len) const;
    void OnBitmapTimer();
    void StopBitmapTimer();
};

#endif
