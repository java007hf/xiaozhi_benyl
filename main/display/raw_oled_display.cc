#include "raw_oled_display.h"

#include <algorithm>
#include <cctype>

#include <esp_log.h>

#define TAG "RawOledDisplay"

RawOledDisplay::RawOledDisplay(ssd1306_spi_handle_t oled, int width, int height)
    : oled_(oled) {
    width_ = width;
    height_ = height;
    mutex_ = xSemaphoreCreateMutex();
    status_ = "INITIALIZING";
}

RawOledDisplay::~RawOledDisplay() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
}

bool RawOledDisplay::Lock(int timeout_ms) {
    if (mutex_ == nullptr) {
        return true;
    }
    TickType_t ticks = timeout_ms <= 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(mutex_, ticks) == pdTRUE;
}

void RawOledDisplay::Unlock() {
    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }
}

void RawOledDisplay::SetupUI() {
    Display::SetupUI();
    DisplayLockGuard lock(this);
    Render();
}

void RawOledDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    status_ = Sanitize(status, 18);
    notification_.clear();
    Render();
}

void RawOledDisplay::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);
    notification_ = Sanitize(notification, 18);
    Render();
}

void RawOledDisplay::SetEmotion(const char* emotion) {
    ESP_LOGD(TAG, "SetEmotion: %s", emotion ? emotion : "");
}

void RawOledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    role_ = Sanitize(role, 10);
    message_ = Sanitize(content, 20);
    Render();
}

void RawOledDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    role_.clear();
    message_.clear();
    Render();
}

void RawOledDisplay::SetPowerSaveMode(bool on) {
    DisplayLockGuard lock(this);
    power_save_ = on;
    if (power_save_) {
        ssd1306_spi_clear(oled_);
        ssd1306_spi_flush(oled_);
        return;
    }
    Render();
}

void RawOledDisplay::Render() {
    if (oled_ == nullptr || power_save_) {
        return;
    }

    ssd1306_spi_clear(oled_);
    ssd1306_spi_draw_rect(oled_, 0, 0, width_, height_);

    DrawTextLine(4, 4, notification_.empty() ? status_ : notification_);

    if (!role_.empty()) {
        DrawTextLine(4, 22, role_ + ":");
    }
    if (!message_.empty()) {
        DrawTextLine(4, 38, message_);
    } else {
        DrawTextLine(22, 36, "XIAOZHI", 2);
    }

    esp_err_t err = ssd1306_spi_flush(oled_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to flush OLED: %s", esp_err_to_name(err));
    }
}

void RawOledDisplay::DrawTextLine(int x, int y, const std::string& text, int scale) {
    if (text.empty()) {
        return;
    }
    ssd1306_spi_draw_text(oled_, x, y, text.c_str(), scale);
}

std::string RawOledDisplay::Sanitize(const char* text, size_t max_len) const {
    if (text == nullptr) {
        return "";
    }

    std::string result;
    result.reserve(max_len);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0' && result.size() < max_len; ++p) {
        unsigned char c = *p;
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
        if (c >= 'a' && c <= 'z') {
            c = static_cast<unsigned char>(std::toupper(c));
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '!' || c == '-' || c == ':') {
            result.push_back(static_cast<char>(c));
        }
    }

    if (result.empty()) {
        return " ";
    }
    return result;
}
