#include "raw_oled_display.h"

#include <algorithm>
#include <cctype>

#include <esp_log.h>
#include <esp_err.h>

#define TAG "RawOledDisplay"

RawOledDisplay::RawOledDisplay(ssd1306_spi_handle_t oled, int width, int height)
    : oled_(oled) {
    width_ = width;
    height_ = height;
    mutex_ = xSemaphoreCreateMutex();
    status_ = "INITIALIZING";

    esp_timer_create_args_t bitmap_timer_args = {
        .callback = [](void* arg) {
            static_cast<RawOledDisplay*>(arg)->OnBitmapTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "raw_oled_bitmap",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&bitmap_timer_args, &bitmap_timer_));
}

static int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

static bool IsBitmapPixelOn(const std::string& row, int width, int x) {
    if (x < 0 || x >= width || row.empty()) {
        return false;
    }

    int total_bits = static_cast<int>(row.size()) * 4;
    int bit_index = total_bits - width + x;
    if (bit_index < 0 || bit_index >= total_bits) {
        return false;
    }

    int hex_index = bit_index / 4;
    int bit_in_nibble = 3 - (bit_index % 4);
    int value = HexValue(row[hex_index]);
    return (value & (1 << bit_in_nibble)) != 0;
}

RawOledDisplay::~RawOledDisplay() {
    if (bitmap_timer_ != nullptr) {
        esp_timer_stop(bitmap_timer_);
        esp_timer_delete(bitmap_timer_);
    }
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

void RawOledDisplay::ShowBitmap(const char* title, int bitmap_width, int bitmap_height, const std::vector<std::string>& rows, int duration_ms) {
    (void)title;
    if (oled_ == nullptr || power_save_) {
        return;
    }
    if (bitmap_width <= 0 || bitmap_height <= 0 || rows.size() < static_cast<size_t>(bitmap_height)) {
        ESP_LOGW(TAG, "Invalid bitmap payload: %dx%d rows=%u", bitmap_width, bitmap_height, static_cast<unsigned>(rows.size()));
        return;
    }

    DisplayLockGuard lock(this);
    bitmap_overlay_active_ = true;
    StopBitmapTimer();

    ssd1306_spi_clear(oled_);

    int available_width = width_;
    int available_height = height_;
    int scale = std::max(1, std::min(available_width / bitmap_width, available_height / bitmap_height));
    int draw_width = bitmap_width * scale;
    int draw_height = bitmap_height * scale;
    int origin_x = std::max(0, (width_ - draw_width) / 2);
    int origin_y = std::max(0, (height_ - draw_height) / 2);

    for (int y = 0; y < bitmap_height; ++y) {
        const auto& row = rows[y];
        for (int x = 0; x < bitmap_width; ++x) {
            if (!IsBitmapPixelOn(row, bitmap_width, x)) {
                continue;
            }
            for (int sy = 0; sy < scale; ++sy) {
                for (int sx = 0; sx < scale; ++sx) {
                    ssd1306_spi_draw_pixel(oled_, origin_x + x * scale + sx, origin_y + y * scale + sy, true);
                }
            }
        }
    }

    esp_err_t err = ssd1306_spi_flush(oled_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to flush OLED bitmap: %s", esp_err_to_name(err));
    }

    int safe_duration_ms = std::max(1000, duration_ms);
    bitmap_until_us_ = esp_timer_get_time() + static_cast<int64_t>(safe_duration_ms) * 1000;
    if (bitmap_timer_ != nullptr) {
        err = esp_timer_start_once(bitmap_timer_, static_cast<uint64_t>(safe_duration_ms) * 1000ULL);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start bitmap timer: %s", esp_err_to_name(err));
        }
    }
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
        StopBitmapTimer();
        bitmap_overlay_active_ = false;
        bitmap_until_us_ = 0;
        ssd1306_spi_clear(oled_);
        ssd1306_spi_flush(oled_);
        return;
    }
    Render();
}

void RawOledDisplay::Render() {
    if (oled_ == nullptr || power_save_ || bitmap_overlay_active_) {
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

void RawOledDisplay::OnBitmapTimer() {
    DisplayLockGuard lock(this);
    int64_t now = esp_timer_get_time();
    if (bitmap_overlay_active_ && bitmap_until_us_ > now) {
        esp_timer_start_once(bitmap_timer_, static_cast<uint64_t>(bitmap_until_us_ - now));
        return;
    }
    bitmap_overlay_active_ = false;
    bitmap_until_us_ = 0;
    Render();
}

void RawOledDisplay::StopBitmapTimer() {
    if (bitmap_timer_ != nullptr) {
        esp_timer_stop(bitmap_timer_);
    }
}
