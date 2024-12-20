/**
 ** This file is part of the libgamepad project.
 ** Copyright 2020 univrsal <universailp@web.de>.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as
 ** published by the Free Software Foundation, either version 3 of the
 ** License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#pragma once

#define LGP_ENABLE_JSON
#define LGP_UNUSED(a) ((void)a)
#ifdef WIN32
#define LGP_WINDOWS 1
#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif
#elif __linux__
#define LGP_LINUX 1
#define LGP_UNIX 1
#elif __APPLE__
#define LGP_MACOS 1
#define LGP_UNIX 1
#else
#define LGP_UNIX 1
#endif
