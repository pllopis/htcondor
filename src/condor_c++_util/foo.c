/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#define _POSIX_SOURCE

#include "condor_common.h"
#include "user_log.h"

main( int argc, char *argv[] )
{
	int		i;
	LP		*lp;

	int		cluster, proc, subproc;

	if( argc != 4 ) {
		fprintf( stderr, "Usage: %s cluster proc subproc\n", argv[0] );
		exit( 1 );
	}

	cluster = atoi( argv[1] );
	proc = atoi( argv[2] );
	subproc = atoi( argv[3] );

	lp =  InitUserLog( "condor", "/tmp/condor", cluster, proc, subproc );
	PutUserLog( lp, "At line %d in %s\n", __LINE__, __FILE__ );
	PutUserLog(lp, "At line %d in %s\n", __LINE__, __FILE__ );

	BeginUserLogBlock( lp );
	for( i=0; i<10; i++ ) {
		PutUserLog( lp, "This is line %d of output\n", i );
	}
	EndUserLogBlock( lp );

	exit( 0 );
}

extern "C" int SetSyscalls( int mode ) { return mode; }

