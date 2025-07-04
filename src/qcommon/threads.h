/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#ifndef __THREADS_H
#define __THREADS_H

#include "../game/q_shared.h"
#include "qcommon.h"

#ifdef __linux__
	#include <gnu/lib-names.h>
	#include <pthread.h>
	#include <dlfcn.h>
#else //WIN32
	#include <process.h>
//#elif defined( __MACOS__ )
//	#define LIBPTHREAD_SO "/usr/lib/libpthread.dylib"
//#elif defined( __APPLE__ )
//	#define LIBPTHREAD_SO "/usr/lib/libpthread.dylib"
#endif

//
// *_threads.c
//
void Threads_Init(void);
int Threads_Create(void* (*thread_function)(void*), void* arguments);

#endif // ~!__THREADS_H
