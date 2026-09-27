#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_log.h"
#include "esp_err.h"

/* GPIO definitions */
#define MOSI    23
#define SCK     18
#define CS       5
#define RST     17
#define DC      16

#define SSD1322_WIDTH   256
#define SSD1322_HEIGHT   64

enum
{
    LO,
    HI
};

spi_device_handle_t spi;

/* 8192 byte framebuffer */
static uint8_t framebuffer[
    SSD1322_WIDTH * SSD1322_HEIGHT / 2
];

/************************************************************
 * SPI callback
 ************************************************************/
static void IRAM_ATTR pre_cb(spi_transaction_t *t)
{
    gpio_set_level(DC, (int)(intptr_t)t->user);
}

/************************************************************
 * Send command
 ************************************************************/
static void ssd1322_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0
    };

    ESP_ERROR_CHECK(
        spi_device_transmit(spi, &t)
    );
}

/************************************************************
 * Send data
 ************************************************************/
static void ssd1322_data(uint8_t data)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .user = (void *)1
    };

    ESP_ERROR_CHECK(
        spi_device_transmit(spi, &t)
    );
}

/************************************************************
 * Reset display
 ************************************************************/
static void ssd1322_reset(void)
{
    gpio_set_level(RST, LO);
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_set_level(RST, HI);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/************************************************************
 * Initialise SSD1322
 ************************************************************/
static void ssd1322_init(void)
{
    ssd1322_cmd(0xFD);
    ssd1322_data(0x12);

    ssd1322_cmd(0xAE);

    ssd1322_cmd(0xB3);
    ssd1322_data(0x91);

    ssd1322_cmd(0xCA);
    ssd1322_data(0x3F);

    ssd1322_cmd(0xA2);
    ssd1322_data(0x00);

    ssd1322_cmd(0xA1);
    ssd1322_data(0x00);

    ssd1322_cmd(0xA0);
    ssd1322_data(0x14);
    ssd1322_data(0x11);

    ssd1322_cmd(0xB5);
    ssd1322_data(0x00);

    ssd1322_cmd(0xAB);
    ssd1322_data(0x01);

    ssd1322_cmd(0xC1);
    ssd1322_data(0x9F);

    ssd1322_cmd(0xC7);
    ssd1322_data(0x0F);

    ssd1322_cmd(0xB9);

    ssd1322_cmd(0xA6);

    ssd1322_cmd(0xAF);
}

/************************************************************
 * Clear framebuffer
 ************************************************************/
static void ssd1322_clear_buffer(void)
{
    memset(framebuffer, 0x00, sizeof(framebuffer));
}

/************************************************************
 * Draw pixel
 ************************************************************/
static void ssd1322_draw_pixel(
    uint16_t x,
    uint16_t y,
    uint8_t gray)
{
    if(x >= SSD1322_WIDTH)
        return;

    if(y >= SSD1322_HEIGHT)
        return;

    gray &= 0x0F;

    uint32_t index =
        (y * SSD1322_WIDTH + x) / 2;

    if(x & 1)
    {
        framebuffer[index] &= 0xF0;
        framebuffer[index] |= gray;
    }
    else
    {
        framebuffer[index] &= 0x0F;
        framebuffer[index] |= (gray << 4);
    }
}

/************************************************************
 * Copy framebuffer to OLED
 ************************************************************/
static void ssd1322_update(void)
{
    ssd1322_cmd(0x15);
    ssd1322_data(0x1C);
    ssd1322_data(0x5B);

    ssd1322_cmd(0x75);
    ssd1322_data(0x00);
    ssd1322_data(0x3F);

    ssd1322_cmd(0x5C);

    spi_transaction_t t = {
        .length = sizeof(framebuffer) * 8,
        .tx_buffer = framebuffer,
        .user = (void *)1
    };

    ESP_ERROR_CHECK(
        spi_device_transmit(spi, &t)
    );
}

/************************************************************
 * Main
 ************************************************************/
void app_main(void)
{
    gpio_config_t output_conf = {
        .pin_bit_mask =
            (1ULL << RST) |
            (1ULL << DC),

        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(&output_conf)
    );

    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI,
        .miso_io_num = -1,
        .sclk_io_num = SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,

        .max_transfer_sz = 8192
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            SPI2_HOST,
            &buscfg,
            SPI_DMA_CH_AUTO)
    );

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10000000,
        .mode = 0,
        .spics_io_num = CS,
        .queue_size = 7,
        .pre_cb = pre_cb
    };

    ESP_ERROR_CHECK(
        spi_bus_add_device(
            SPI2_HOST,
            &devcfg,
            &spi)
    );

    ssd1322_reset();
    ssd1322_init();

    ssd1322_clear_buffer();

    /* Draw a horizontal line */
    for(int x = 0; x < 256; x++)
    {
        ssd1322_draw_pixel(
            x,
            32,
            15
        );
    }

    ssd1322_update();

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}