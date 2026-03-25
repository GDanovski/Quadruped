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

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_MOVE_CONTROLLER_MODULE src/move_controller.c)
zephyr_library_include_directories(include)
zephyr_include_directories(include)
