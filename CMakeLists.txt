# Copyright (C) 2026 Georgi Danovski
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_EXTRA_MODULES
	${CMAKE_CURRENT_SOURCE_DIR}/modules/leg
	${CMAKE_CURRENT_SOURCE_DIR}/modules/quadruped
	${CMAKE_CURRENT_SOURCE_DIR}/modules/voltage_regulator
	${ZEPHYR_EXTRA_MODULES}
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_quadrapod)

target_sources(app PRIVATE src/main.c)
