/*
 * Copyright (C) 2026 Georgi Danovski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <nrfx.h>
#include <zephyr/logging/log.h>

#include "voltage_regulator/voltage_regulator.h"

LOG_MODULE_REGISTER(voltage_regulator);

/**
 * @brief Internal helper that writes UICR REGOUT0 to 3.3V and resets.
 */
static void configure_regout0_3v3(void)
{
#if defined(UICR_REGOUT0_VOUT_Msk) && defined(UICR_REGOUT0_VOUT_3V3)
    uint32_t regout0 = NRF_UICR->REGOUT0;
    uint32_t vout = (regout0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos;

    if (vout == UICR_REGOUT0_VOUT_3V3)
    {
        return;
    }

    LOG_WRN("Programming UICR REGOUT0 to 3.3V; resetting");

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
    }

    NRF_UICR->REGOUT0 = (regout0 & ~UICR_REGOUT0_VOUT_Msk) |
                        (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
    }

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
    }

    NVIC_SystemReset();
#else
    LOG_WRN("REGOUT0 3.3V configuration macros are unavailable for this target");
#endif
}

void voltage_regulator_configure_regout0_3v3(void)
{
    configure_regout0_3v3();
}
