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

#ifndef VOLTAGE_REGULATOR_VOLTAGE_REGULATOR_H_
#define VOLTAGE_REGULATOR_VOLTAGE_REGULATOR_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Configure REGOUT0 to 3.3V when supported by the target.
 *
 * Programs UICR REGOUT0 to 3.3V if needed and triggers a system reset so
 * the new voltage setting is applied.
 */
#if defined(CONFIG_VOLTAGE_REGULATOR_MODULE)
    void voltage_regulator_configure_regout0_3v3(void);
#else
static inline void voltage_regulator_configure_regout0_3v3(void)
{
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* VOLTAGE_REGULATOR_VOLTAGE_REGULATOR_H_ */
