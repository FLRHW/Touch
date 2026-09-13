#ifndef INC_TOUCH_H_
#define INC_TOUCH_H_

#include <stdint.h>
#include <stdbool.h>

#define TOUCH_X_MIN    460
#define TOUCH_X_MAX    1730

#define TOUCH_Y_MIN    265
#define TOUCH_Y_MAX    1680

#define TOUCH_WIDTH    240
#define TOUCH_HEIGHT   320

bool Touch_GetPoint(uint16_t *x, uint16_t *y);

void Touch_GetRaw(uint16_t *x,
                  uint16_t *y,
                  uint16_t *z1,
                  uint16_t *z2);

#endif
