#include "ia_css_debug.h"
#include "ia_css_tdf.host.h"

const int16_t g_pyramid[8][8] = {
{128, 384, 640, 896, 896, 640, 384, 128},
{384, 1152, 1920, 2688, 2688, 1920, 1152, 384},
{640, 1920, 3200, 4480, 4480, 3200, 1920, 640},
{896, 2688, 4480, 6272, 6272, 4480, 2688, 896},