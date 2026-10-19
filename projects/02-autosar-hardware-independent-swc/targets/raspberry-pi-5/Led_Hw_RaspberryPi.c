#include <stdio.h>
#include <gpiod.h>

#include "Led_Hw.h"

#define LED_LINE_NAME "GPIO17"

static struct gpiod_chip *gpioChip = NULL;
static struct gpiod_line_request *ledRequest = NULL;
static unsigned int ledOffset = 0;

static int find_led_gpio(void)
{
    char chipPath[32];

    for (int i = 0; i < 16; i++)
    {
        snprintf(chipPath, sizeof(chipPath), "/dev/gpiochip%d", i);

        gpioChip = gpiod_chip_open(chipPath);

        if (gpioChip == NULL)
        {
            continue;
        }

        int offset =
            gpiod_chip_get_line_offset_from_name(gpioChip, LED_LINE_NAME);

        if (offset >= 0)
        {
            ledOffset = (unsigned int)offset;

            printf("Found %s on %s, offset %u\n",
                   LED_LINE_NAME,
                   chipPath,
                   ledOffset);

            return 0;
        }

        gpiod_chip_close(gpioChip);
        gpioChip = NULL;
    }

    return -1;
}

void Led_Hw_Init(void)
{
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *lineConfig;
    struct gpiod_request_config *requestConfig;

    if (find_led_gpio() != 0)
    {
        printf("GPIO17 not found\n");
        return;
    }

    settings = gpiod_line_settings_new();
    lineConfig = gpiod_line_config_new();
    requestConfig = gpiod_request_config_new();

    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_OUTPUT);

    gpiod_line_settings_set_output_value(
        settings,
        GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config_add_line_settings(
        lineConfig,
        &ledOffset,
        1,
        settings);

    gpiod_request_config_set_consumer(
        requestConfig,
        "AUTOSAR_LED");

    ledRequest = gpiod_chip_request_lines(
        gpioChip,
        requestConfig,
        lineConfig);

    gpiod_request_config_free(requestConfig);
    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (ledRequest == NULL)
    {
        printf("Failed to request GPIO17\n");
        return;
    }

    printf("Raspberry Pi LED initialized\n");
}

void Led_Hw_Write(boolean state)
{
    if (ledRequest == NULL)
    {
        return;
    }

    gpiod_line_request_set_value(
        ledRequest,
        ledOffset,
        state == TRUE
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE);
}