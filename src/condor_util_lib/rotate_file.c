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

#include "condor_common.h"
#include "condor_debug.h"

int
rotate_file(const char *old_filename, const char *new_filename)
{
#if defined(WIN32)
	/* We must use MoveFileEx on NT because rename can not overwrite
	   an existing file. */
	if (MoveFileEx(old_filename, new_filename,
				   MOVEFILE_REPLACE_EXISTING) == 0) {
		dprintf(D_ALWAYS, "MoveFileEx(%s,%s) failed with error %d\n",
				old_filename, new_filename, GetLastError());
		return -1;
	}
#else
	if (rename(old_filename, new_filename) < 0) {
		dprintf(D_ALWAYS, "rename(%s, %s) failed with errno %d\n",
				old_filename, new_filename, errno);
		return -1;
	}
#endif

	return 0;
}
