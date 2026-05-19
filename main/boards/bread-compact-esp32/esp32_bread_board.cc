#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "display/raw_oled_display.h"
#include "ssd1306_spi.h"

#include <esp_log.h>

#define TAG "ESP32-MarsbearSupport"

class CompactWifiBoard : public WifiBoard {
private:
    Button boot_button_;
    Button asr_button_;

    ssd1306_spi_handle_t oled_ = nullptr;
    Display* display_ = nullptr;

    void InitializeSsd1306Display() {
        ssd1306_spi_config_t config = {
            .host = DISPLAY_SPI_HOST,
            .pin_sclk = DISPLAY_SPI_SCLK_PIN,
            .pin_mosi = DISPLAY_SPI_MOSI_PIN,
            .pin_rst = DISPLAY_SPI_RST_PIN,
            .pin_dc = DISPLAY_SPI_DC_PIN,
            .clock_speed_hz = 8 * 1000 * 1000,
        };

        esp_err_t err = ssd1306_spi_new(&config, &oled_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize raw SPI SSD1306: %s", esp_err_to_name(err));
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGI(TAG, "Raw SPI SSD1306 initialized");
        display_ = new RawOledDisplay(oled_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    void InitializeButtons() {
        
        // 配置 GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // 设置需要配置的 GPIO 引脚
            .mode = GPIO_MODE_OUTPUT,           // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE      // 禁用中断
        };
        gpio_config(&io_conf);  // 应用配置

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word="你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

    public:
    CompactWifiBoard() : WifiBoard(), boot_button_(BOOT_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override 
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

};

DECLARE_BOARD(CompactWifiBoard);
