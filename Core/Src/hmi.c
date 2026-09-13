#include "main.h"
#include "hmi.h"
#include "ILI9341_driver.h"


/*
 * ------------------------------------------------------------
 * Simple static UI preview
 * 320 x 240 landscape
 * ------------------------------------------------------------
 */


/* Memory buttons */
#define MEM_Y       154
#define MEM_W       66
#define MEM_H       74

#define M1_X        10
#define M2_X        86
#define M3_X        162


/* Lift / Lower buttons */
#define RIGHT_X     240
#define RIGHT_W     70
#define RIGHT_H     88

#define LIFT_Y      10
#define LOWER_Y     140


/* Settings/Menu button */
#define MENU_X      10
#define MENU_Y      10
#define MENU_W      50
#define MENU_H      50





static void draw_menu_icon(void)
{
    /*
     * Settings/menu button outline.
     */
    ILI9341_DrawRect(
        MENU_X,
        MENU_Y,
        MENU_W,
        MENU_H,
        3,
        WHITE
    );

    /*
     * Three horizontal menu bars.
     */
    ILI9341_FillRect(21, 23, 28, 4, WHITE);
    ILI9341_FillRect(21, 33, 28, 4, WHITE);
    ILI9341_FillRect(21, 43, 28, 4, WHITE);
}


static void draw_memory_button(uint16_t x,
                               const char *text)
{
    ILI9341_DrawRect(
        x,
        MEM_Y,
        MEM_W,
        MEM_H,
        3,
        WHITE
    );

    /*
     * 2 characters at scale 2:
     *
     * 2 * 11 * 2 = 44 pixels wide
     * 23 * 2     = 46 pixels high
     *
     * Centre inside 66 x 74 button.
     */
    ILI9341_DrawStringScale(
        x + 11,
        MEM_Y + 14,
        text,
        2,
        WHITE,
        BLACK
    );
}


static void draw_lift_button(void)
{
    ILI9341_DrawRect(
        RIGHT_X,
        LIFT_Y,
        RIGHT_W,
        RIGHT_H,
        3,
        WHITE
    );

    ILI9341_DrawUpArrow(
        RIGHT_X + (RIGHT_W / 2),
        20,
        WHITE
    );

    ILI9341_DrawStringNoDMA(
        RIGHT_X + 8,
        61,
        "LIFT",
        WHITE,
        BLACK
    );
}


static void draw_lower_button(void)
{
    ILI9341_DrawRect(
        RIGHT_X,
        LOWER_Y,
        RIGHT_W,
        RIGHT_H,
        3,
        WHITE
    );

    ILI9341_DrawStringNoDMA(
        RIGHT_X + 2,
        151,
        "LOWER",
        WHITE,
        BLACK
    );

    ILI9341_DrawDownArrow(
        RIGHT_X + (RIGHT_W / 2),
        193,
        WHITE
    );
}


static void draw_screen(void)
{
    /*
     * Background
     */
    ILI9341_FillScreen(BLACK);


    /*
     * Menu/settings
     */
    draw_menu_icon();


    /*
     * Height readout.
     *
     * Existing 11x23 font enlarged 2x:
     *
     * "90 cm" = 5 chars
     * width  = 5 * 11 * 2 = 110 pixels
     * height = 23 * 2      = 46 pixels
     */
    ILI9341_DrawStringScale(
        105,
        26,
        "90 cm",
        2,
        WHITE,
        BLACK
    );


    /*
     * Lift / Lower
     */
    draw_lift_button();
    draw_lower_button();


    /*
     * Memory buttons
     */
    draw_memory_button(M1_X, "M1");
    draw_memory_button(M2_X, "M2");
    draw_memory_button(M3_X, "M3");
}


void hmi_main(void)
{
    ILI9341_Init();

    draw_screen();

    /*
     * Static UI preview.
     *
     * No touch handling yet.
     */
    while (1)
    {
        HAL_Delay(1000);
    }
}
