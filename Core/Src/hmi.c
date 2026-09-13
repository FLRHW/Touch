#include "main.h"
#include "hmi.h"
#include "ILI9341_driver.h"
#include "touch.h"
#include <stdbool.h>


#define BUTTON1_X 20
#define BUTTON1_Y 60
#define BUTTON1_W 200
#define BUTTON1_H 90

#define BUTTON2_X 20
#define BUTTON2_Y 180
#define BUTTON2_W 200
#define BUTTON2_H 90


static bool inside_button(uint16_t x, uint16_t y,
                          uint16_t bx, uint16_t by,
                          uint16_t bw, uint16_t bh)
{
    return ((x >= bx) &&
            (x < (bx + bw)) &&
            (y >= by) &&
            (y < (by + bh)));
}


static void draw_screen(void)
{
    ILI9341_FillScreen(BLACK);

    ILI9341_DrawStringNoDMA(
        65, 20,
        "TOUCH TEST",
        WHITE,
        BLACK
    );

    /*
     * Button 1
     */
    ILI9341_DrawStringNoDMA(
        55, 95,
        "[ BUTTON 1 ]",
        WHITE,
        BLUE
    );

    /*
     * Button 2
     */
    ILI9341_DrawStringNoDMA(
        55, 215,
        "[ BUTTON 2 ]",
        WHITE,
        GREEN
    );
}


void hmi_main(void)
{
    uint16_t touch_x;
    uint16_t touch_y;

    ILI9341_Init();

    draw_screen();

    while (1)
    {
        if (Touch_GetPoint(&touch_x, &touch_y))
        {
            /*
             * BUTTON 1
             */
            if (inside_button(touch_x,
                              touch_y,
                              BUTTON1_X,
                              BUTTON1_Y,
                              BUTTON1_W,
                              BUTTON1_H))
            {
                /*
                 * Make the text BLACK on a WHITE background.
                 * This gives us a very obvious visual change.
                 */
                ILI9341_DrawStringNoDMA(
                    55, 95,
                    "[ BUTTON 1 ]",
                    BLACK,
                    WHITE
                );

                /*
                 * Wait until finger/stylus is released.
                 */
                while (Touch_GetPoint(&touch_x, &touch_y))
                {
                    HAL_Delay(20);
                }

                /*
                 * Restore normal appearance.
                 */
                ILI9341_DrawStringNoDMA(
                    55, 95,
                    "[ BUTTON 1 ]",
                    WHITE,
                    BLUE
                );
            }

            /*
             * BUTTON 2
             */
            else if (inside_button(touch_x,
                                   touch_y,
                                   BUTTON2_X,
                                   BUTTON2_Y,
                                   BUTTON2_W,
                                   BUTTON2_H))
            {
                /*
                 * Make the text BLACK on a WHITE background.
                 */
                ILI9341_DrawStringNoDMA(
                    55, 215,
                    "[ BUTTON 2 ]",
                    BLACK,
                    WHITE
                );

                /*
                 * Wait until finger/stylus is released.
                 */
                while (Touch_GetPoint(&touch_x, &touch_y))
                {
                    HAL_Delay(20);
                }

                /*
                 * Restore normal appearance.
                 */
                ILI9341_DrawStringNoDMA(
                    55, 215,
                    "[ BUTTON 2 ]",
                    WHITE,
                    GREEN
                );
            }
        }

        HAL_Delay(20);
    }
}
