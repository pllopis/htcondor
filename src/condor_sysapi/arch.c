/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "match_prefix.h"
#include "sysapi.h"

/* ok, here's the deal with these. For some reason, gcc on IRIX is just old 
	enough to where it doesn't understand things like static const FALSE = 0.
	So these two things used to be defined as the above, but it would fail with
	a 'initializer element is not constant' during compile. I have defined
	them as to make the preprocessor do the work instead of a const int.
	-psilord 06/01/99 */
#define TRUE 1
#define FALSE 0

static char *_FileName_ = __FILE__;

#if defined(WIN32)

char *
sysapi_condor_arch()
{
	static char answer[1024];	
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	switch(info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		sprintf(answer, "INTEL");
		break;
	case PROCESSOR_ARCHITECTURE_MIPS:
		sprintf(answer, "MIPS");
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
		sprintf(answer, "ALPHA");
		break;
	case PROCESSOR_ARCHITECTURE_PPC:
		sprintf(answer, "PPC");
		break;
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	default:
		sprintf(answer, "UNKNOWN");
		break;
	}

	return answer;
}


char *
sysapi_uname_arch() 
{
	return sysapi_condor_arch();
}


char *
sysapi_opsys()
{
	static char answer[1024];
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx(&info) > 0) {
		switch(info.dwPlatformId) {
		case VER_PLATFORM_WIN32s:
			sprintf(answer, "WIN32s%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			sprintf(answer, "WIN32%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_NT:
			sprintf(answer, "WINNT%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		default:
			sprintf(answer, "UNKNOWN");
			break;
		}
	} else {
		sprintf(answer, "ERROR");
	}

	return answer;
}

#else 

static int arch_inited = FALSE;
static char* arch = NULL;
static char* uname_arch = NULL;
static char* opsys = NULL;

#ifdef HPUX
char* get_hpux_arch( struct utsname* );
#endif

void
init_arch() 
{
	struct utsname buf;	
	char tmp[64];

	if( uname(&buf) < 0 ) {
		return;
	}

	uname_arch = strdup( buf.machine );
	if( !uname_arch ) {
		EXCEPT( "Out of memory!" );
	}

#ifdef HPUX
	/*
	  On HPUX, to figure out if we're HPPA1 or HPPA2, we have to
	  lookup what we get back from uname in a file that lists all the
	  different HP models and what kinds of CPU each one has.  We do
	  this in a seperate function to keep this function somewhat
	  readable.   -Derek Wright 7/20/98
	*/

	arch = get_hpux_arch( &buf );
#else 
		// Get ARCH
	if( !strcmp(buf.machine, "alpha") ) {
		sprintf( tmp, "ALPHA" );
	} else if( !strcmp(buf.machine, "i86pc") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i686") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i586") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i486") ) {
		sprintf( tmp, "INTEL" );
	} else if( match_prefix(buf.sysname, "IRIX") ) {
		sprintf( tmp, "SGI" );
	} else if( !strcmp(buf.machine, "sun4u") ) {
		sprintf( tmp, "SUN4u" );
	} else if( !strcmp(buf.machine, "sun4m") ) {
		sprintf( tmp, "SUN4x" );
	} else if( !strcmp(buf.machine, "sun4c") ) {
		sprintf( tmp, "SUN4x" );
	} else {
			// Unknown, just use what uname gave:
		sprintf( tmp, buf.machine );
	}
	arch = strdup( tmp );
	if( !arch ) {
		EXCEPT( "Out of memory!" );
	}

#endif /* HPUX */

		// Get OPSYS
	if( !strcmp(buf.sysname, "Linux") ) {
		if( access("/lib/libc.so.6", R_OK) != 0 ) {
			sprintf( tmp, "LINUX" );
		} else { 
			sprintf( tmp, "LINUX-GLIBC" );
		}
	} else if( !strcmp(buf.sysname, "SunOS") ) {
		if( !strcmp(buf.release, "5.6") ) {
			sprintf( tmp, "SOLARIS26" );
		} else if ( !strcmp(buf.release, "5.5.1") ) {
			sprintf( tmp, "SOLARIS251" );
		} else if ( !strcmp(buf.release, "5.5") ) {
			sprintf( tmp, "SOLARIS25" );
		} else {
			sprintf( tmp, "SOLARIS%s", buf.release );
		}
	} else if( !strcmp(buf.sysname, "OSF1") ) {
		sprintf( tmp, "OSF1" );
	} else if( !strcmp(buf.sysname, "HP-UX") ) {
		if( !strcmp(buf.release, "B.10.20") ) {
			sprintf( tmp, "HPUX10" );
		} else {
			sprintf( tmp, "HPUX%s", buf.release );
		}
	} else if( match_prefix(buf.sysname, "IRIX") ) {
		if( buf.release[0] == '6' ) {
			sprintf( tmp, "IRIX6" );
		} else {
			sprintf( tmp, "IRIX%s", buf.release );
		}
	} else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s%s", buf.sysname, buf.release );
	}
	opsys = strdup( tmp );
	if( !opsys ) {
		EXCEPT( "Out of memory!" );
	}
	arch_inited = TRUE;
}


char *
sysapi_condor_arch()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return arch;
}


char *
sysapi_opsys()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys;
}


char *
sysapi_uname_arch()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return uname_arch;
}


#if defined(HPUX)
char*
get_hpux_arch( struct utsname *buf )
{
    FILE* fp;
	static char *file = "/opt/langtools/lib/sched.models";
	char machinfo[4096], line[128];
	char *model;
	char cputype[128], cpumodel[128];
	int len, found_it = FALSE;

	model = strchr( buf->machine, '/' );
	model++;
	len = strlen( model );

	fp = fopen( file, "r" );
	if( ! fp ) {
	    return NULL;
	}	
	while( ! feof(fp) ) {
	    fgets( line, 128, fp );
		if( !strncmp(line, model, len) ) {
		    found_it = TRUE;
		    break;
		}
	}
	fclose( fp );
	if( found_it ) {
  	    sscanf( line, "%s\t%s", cpumodel, cputype );
		if( !strcmp(cputype, "2.0") ) {
		    arch = strdup( "HPPA2" );
		} else {
		    arch = strdup( "HPPA1" );
		}
		if( !arch ) {
			EXCEPT( "Out of memory!" );
		}
	}
	return arch;
}
#endif /* HPUX */

#endif /* ! WIN32 */

