/******************************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "file_modified_trigger.h"

#if defined( LINUX )

#include <sys/inotify.h>
#include <poll.h>

FileModifiedTrigger::FileModifiedTrigger( const std::string & f ) :
	filename( f ), initialized( false ), inotify_fd( -1 )
{
	inotify_fd = inotify_init1( IN_NONBLOCK );
	if( inotify_fd == -1 ) {
		dprintf( D_ALWAYS, "FileModifiedTrigger( %s ): inotify_init() failed: %s (%d).\n", filename.c_str(), strerror(errno), errno );
		return;
	}

	int wd = inotify_add_watch( inotify_fd, filename.c_str(), IN_MODIFY );
	if( wd == -1 ) {
		dprintf( D_ALWAYS, "FileModifiedTrigger( %s ): inotify_add_watch() failed: %s (%d).\n", filename.c_str(), strerror( errno ), errno );
		return;
	}

	initialized = true;
}

FileModifiedTrigger::~FileModifiedTrigger() {
	if( initialized && inotify_fd != -1 ) {
		close( inotify_fd );
	}
}


int
FileModifiedTrigger::read_inotify_events( void ) {
	// Magic from 'man inotify'.
	char buf[ sizeof(struct inotify_event) + NAME_MAX + 1 ]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));

	while( true ) {
		ssize_t len = read( inotify_fd, buf, sizeof( buf ) );
		if( len == -1 && errno != EAGAIN ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): failed to ready from inotify fd.\n", filename.c_str() );
			return -1;
		}

		// We're done reading events for now.
		if( len <= 0 ) { return 1; }

		char * ptr = buf;
		for( ; ptr < buf + len; ptr += sizeof(struct inotify_event) + ((struct inotify_event *)ptr)->len ) {
			const struct inotify_event * event = (struct inotify_event *)ptr;
			if(! (event->mask & IN_MODIFY) ) {
				dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): inotify gave me an event I didn't ask for.\n", filename.c_str() );
				return -1;
			}
		}

		// We don't worry about partial reads because we're only watching
		// one file for one event type, and the kernel will coalesce
		// identical events, so we should only ever see one. Nonetheless,
		// we'll verify here that we read only complete events.
		if( ptr != buf + len ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): partial inotify read.\n", filename.c_str() );
			return -1;
		}
	}

	return 1;
}

int
FileModifiedTrigger::wait( int timeout ) {
	if(! initialized) {
		return -1;
	}

	struct pollfd pollfds[1];

	pollfds[0].fd = inotify_fd;
	pollfds[0].events = POLLIN;
	pollfds[0].revents = 0;

	int events = poll( pollfds, 1, timeout );
	switch( events ) {
		case -1:
			return -1;

		case 0:
			return 0;

		default:
			if( pollfds[0].revents & POLLIN ) {
				return read_inotify_events();
			} else {
				dprintf( D_ALWAYS, "FileModifiedTrigger::wait(): poll() returned an event I didn't ask for.\n" );
				return -1;
			}
		break;
	}
}

#else

//
// Stubs for other platforms.  We should replace these with (a) a Windows-
// specific blocking interface and (b) a "unix" (MacOSX) fallback that polls.
//
// Collected as its own block for clarity.
//

FileModifiedTrigger::FileModifiedTrigger( const std::string & f ) :
	filename( f ), initialized( false ) { }

FileModifiedTrigger::~FileModifiedTrigger() { }

int
FileModifiedTrigger::wait( int milliseconds ) {
	return -1;
}

#endif
