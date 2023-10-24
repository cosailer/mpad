// Copyright 2023 CoSailer (@CoSailer)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
 * Feature disable options
 *  These options are also useful to firmware size reduction.
 */

/* disable debug print */
//#define NO_DEBUG

/* disable print */
//#define NO_PRINT

/* disable action features */
//#define NO_ACTION_LAYER
//#define NO_ACTION_TAPPING
//#define NO_ACTION_ONESHOT

#define DEBOUNCE 5

//#define LED_NUM_LOCK_PIN     C6
//#define LED_CAPS_LOCK_PIN    C7
//#define LED_SCROLL_LOCK_PIN  E2

////////////////////////////////////////////////
//################# backlight ##################

//#undef BACKLIGHT_PIN
//#define BACKLIGHT_PINS { B2, B1, B0, B3, F6, E6, F7, D7, F4, C6, C7, E2 }
//#define BACKLIGHT_LEVELS     10
//#define BACKLIGHT_ON_STATE   1
//#define BACKLIGHT_LIMIT_VAL  255


//#define BACKLIGHT_LED_COUNT 3
//#define BACKLIGHT_BREATHING  1
//#define BREATHING_PERIOD     6

//#define LED_PIN_ON_STATE    0

////////////////////////////////////////////////
//################# encoders ###################

#define ENCODERS_PAD_A { F0 }
#define ENCODERS_PAD_B { F1 }

#define ENCODER_RESOLUTION  1


