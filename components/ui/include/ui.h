/* components/ui/include/ui.h */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise GPIO buttons/encoders and start the UI task.
 *
 * Wires hardware inputs to sequencer control callbacks internally.
 */
esp_err_t ui_init(void);

#ifdef __cplusplus
}
#endif