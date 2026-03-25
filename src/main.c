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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "move_controller/move_controller.h"
#include "voltage_regulator/voltage_regulator.h"

#define APP_MOVE_DELAY_MS 500u
#define APP_MOVE_THREAD_STACK_SIZE 1024
#define APP_MOVE_THREAD_PRIORITY 5

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Shared move command state: producer threads update it, movement thread consumes it. */
atomic_t app_move_command = ATOMIC_INIT(MOVE_COMMAND_IDLE);

static void app_move_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (1)
	{
		enum move_command cmd = (enum move_command)atomic_get(&app_move_command);

		if (cmd >= MOVE_COMMAND_COUNT)
		{
			LOG_ERR("Invalid move command in app_move_command: %d", (int)cmd);
			cmd = MOVE_COMMAND_IDLE;
		}

		int ret = move_controller_execute(cmd, APP_MOVE_DELAY_MS);

		if (ret != 0)
		{
			LOG_ERR("move_controller failed: %d", ret);
		}
	}
}

K_THREAD_DEFINE(app_move_tid,
				APP_MOVE_THREAD_STACK_SIZE,
				app_move_thread,
				NULL,
				NULL,
				NULL,
				APP_MOVE_THREAD_PRIORITY,
				0,
				0);

int main(void)
{
	voltage_regulator_configure_regout0_3v3();

	while (1)
	{
		atomic_set(&app_move_command, MOVE_COMMAND_IDLE);
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
