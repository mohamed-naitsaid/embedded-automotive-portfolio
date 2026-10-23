#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>

#include "Led_Hw.h"

#define GPIO_CHIP   "/dev/gpiochip0"
#define LED_OFFSET  17U

static struct gpiod_chip *chip;
static struct gpiod_line_request *request;

void Led_Hw_Init(void)
{
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *lineConfig;
    struct gpiod_request_config *requestConfig;
    unsigned int offset = LED_OFFSET;

    chip = gpiod_chip_open(GPIO_CHIP);

    if (chip == NULL)
    {
        perror("gpiod_chip_open");
        exit(EXIT_FAILURE);
    }

    settings = gpiod_line_settings_new();

    if (settings == NULL)
    {
        fprintf(stderr, "Failed to create line settings\n");
        exit(EXIT_FAILURE);
    }

    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_OUTPUT
    );

    gpiod_line_settings_set_output_value(
        settings,
        GPIOD_LINE_VALUE_INACTIVE
    );

    lineConfig = gpiod_line_config_new();

    if (lineConfig == NULL)
    {
        fprintf(stderr, "Failed to create line config\n");
        exit(EXIT_FAILURE);
    }

    if (gpiod_line_config_add_line_settings(
            lineConfig,
            &offset,
            1,
            settings) < 0)
    {
        fprintf(stderr, "Failed to configure GPIO17\n");
        exit(EXIT_FAILURE);
    }

    requestConfig = gpiod_request_config_new();

    if (requestConfig == NULL)
    {
        fprintf(stderr, "Failed to create request config\n");
        exit(EXIT_FAILURE);
    }

    gpiod_request_config_set_consumer(
        requestConfig,
        "manual-led"
    );

    request = gpiod_chip_request_lines(
        chip,
        requestConfig,
        lineConfig
    );

    if (request == NULL)
    {
        perror("gpiod_chip_request_lines");
        exit(EXIT_FAILURE);
    }

    gpiod_request_config_free(requestConfig);
    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    printf("GPIO17 initialized\n");
}

void Led_Hw_Write(bool state)
{
    enum gpiod_line_value value;

    if (state == true)
    {
        value = GPIOD_LINE_VALUE_ACTIVE;
    }
    else
    {
        value = GPIOD_LINE_VALUE_INACTIVE;
    }

    if (gpiod_line_request_set_value(
            request,
            LED_OFFSET,
            value) < 0)
    {
        perror("gpiod_line_request_set_value");
    }
}
