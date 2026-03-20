/**
 * LVGL Configuration for ESP32-S3 Round Display
 * Copy this file to: Documents\Arduino\libraries\lv_conf.h
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 16 for ESP32 displays */
#define LV_COLOR_DEPTH 16

/* Memory settings */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32U * 1024U)  /* 32KB */

/* Display settings */
#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240
#define LV_DPI_DEF 130

/* Rendering */
#define LV_DISP_DEF_REFR_PERIOD 30  /* 30ms refresh */
#define LV_INDEV_DEF_READ_PERIOD 30

/* Built-in fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

/* Theme */
#define LV_THEME_DEFAULT_DARK 1

/* Logging (for debugging) */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF 1

/* Enable features */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MEM 1
#define LV_USE_ASSERT_STYLE 0

/* Optimize for ESP32 */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* Widgets */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_SLIDER 1

/* Performance */
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_MEM_ALIGN

#endif /* LV_CONF_H */
