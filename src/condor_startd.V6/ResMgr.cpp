/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
 ***************************************************************/


#include "condor_common.h"
#include "startd.h"
#include "condor_classad_namedlist.h"
#include "classad_merge.h"
#include "vm_common.h"
#include "VMRegister.h"
#include <math.h>


ResMgr::ResMgr()
{
	totals_classad = NULL;
	config_classad = NULL;
	up_tid = -1;
	poll_tid = -1;

	m_attr = new MachAttributes;

#if HAVE_BACKFILL
	m_backfill_mgr = NULL;
	m_backfill_shutdown_pending = false;
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr = NULL;
#endif

#if HAVE_HIBERNATION
	m_netif = NetworkAdapterBase::createNetworkAdapter(
		daemonCore->InfoCommandSinfulString (), false );
	m_hibernation_manager = new HibernationManager( );
	if ( m_netif ) {
		m_hibernation_manager->addInterface( *m_netif );
	}
	m_hibernate_tid = -1;
	NetworkAdapterBase	*primary = m_hibernation_manager->getNetworkAdapter();
	if ( NULL == primary ) {
		dprintf( D_FULLDEBUG,
				 "No usable network interface: hibernation disabled\n" );
	}
	else {
		dprintf( D_FULLDEBUG, "Using network interface %s for hibernation\n",
				 primary->interfaceName() );
	}
#endif

	id_disp = NULL;

	nresources = 0;
	resources = NULL;
	type_nums = NULL;
	new_type_nums = NULL;
	is_shutting_down = false;
	cur_time = last_in_use = time( NULL );
}


ResMgr::~ResMgr()
{
	int i;
	if( config_classad ) delete config_classad;
	if( totals_classad ) delete totals_classad;
	if( id_disp ) delete id_disp;
	delete m_attr;

#if HAVE_BACKFILL
	if( m_backfill_mgr ) {
		delete m_backfill_mgr;
	}
#endif

#if HAVE_JOB_HOOKS
	if (m_hook_mgr) {
		delete m_hook_mgr;
	}
#endif

#if HAVE_HIBERNATION
	cancelHibernateTimer();
#endif /* HAVE_HIBERNATION */

	if( resources ) {
		for( i = 0; i < nresources; i++ ) {
			delete resources[i];
		}
		delete [] resources;
	}
	for( i=0; i<max_types; i++ ) {
		if( type_strings[i] ) {
			delete type_strings[i];
		}
	}
	delete [] type_strings;
	delete [] type_nums;
	if( new_type_nums ) {
		delete [] new_type_nums;
	}		
}


void
ResMgr::init_config_classad( void )
{
	if( config_classad ) delete config_classad;
	config_classad = new ClassAd();

		// First, bring in everything we know we need
	configInsert( config_classad, "START", true );
	configInsert( config_classad, "SUSPEND", true );
	configInsert( config_classad, "CONTINUE", true );
	configInsert( config_classad, "PREEMPT", true );
	configInsert( config_classad, "KILL", true );
	configInsert( config_classad, "WANT_SUSPEND", true );
	configInsert( config_classad, "WANT_VACATE", true );
	if( !configInsert( config_classad, "WANT_HOLD", false ) ) {
			// default's to false if undefined
		config_classad->AssignExpr("WANT_HOLD","False");
	}
	configInsert( config_classad, "WANT_HOLD_REASON", false );
	configInsert( config_classad, "WANT_HOLD_SUBCODE", false );
	configInsert( config_classad, "CLAIM_WORKLIFE", false );
	configInsert( config_classad, ATTR_MAX_JOB_RETIREMENT_TIME, false );

		// Now, bring in things that we might need
	configInsert( config_classad, "PERIODIC_CHECKPOINT", false );
	configInsert( config_classad, "RunBenchmarks", false );
	configInsert( config_classad, ATTR_RANK, false );
	configInsert( config_classad, "SUSPEND_VANILLA", false );
	configInsert( config_classad, "CONTINUE_VANILLA", false );
	configInsert( config_classad, "PREEMPT_VANILLA", false );
	configInsert( config_classad, "KILL_VANILLA", false );
	configInsert( config_classad, "WANT_SUSPEND_VANILLA", false );
	configInsert( config_classad, "WANT_VACATE_VANILLA", false );
#if HAVE_BACKFILL
	configInsert( config_classad, "START_BACKFILL", false );
	configInsert( config_classad, "EVICT_BACKFILL", false );
#endif /* HAVE_BACKFILL */
#if HAVE_JOB_HOOKS
	configInsert( config_classad, ATTR_FETCH_WORK_DELAY, false );
#endif /* HAVE_JOB_HOOKS */
#if HAVE_HIBERNATION
	configInsert( config_classad, "HIBERNATE", false );
	configInsert( config_classad, "HIBERNATE_CHECK_INTERVAL", false );
	configInsert( config_classad, "OFFLINE_EXPIRE_AD_AFTER", false );
#endif /* HAVE_HIBERNATION */

		// Next, try the IS_OWNER expression.  If it's not there, give
		// them a resonable default, instead of leaving it undefined. 
	if( ! configInsert(config_classad, ATTR_IS_OWNER, false) ) {
		config_classad->AssignExpr( ATTR_IS_OWNER, "(START =?= False)" );
	}
		// Next, try the CpuBusy expression.  If it's not there, try
		// what's defined in cpu_busy (for backwards compatibility).  
		// If that's not there, give them a default of "False",
		// instead of leaving it undefined.
	if( ! configInsert(config_classad, ATTR_CPU_BUSY, false) ) {
		if( ! configInsert(config_classad, "cpu_busy", ATTR_CPU_BUSY,
						   false) ) { 
			config_classad->Assign( ATTR_CPU_BUSY, false );
		}
	}

	// Publish all DaemonCore-specific attributes, which also handles
	// STARTD_ATTRS for us.
	daemonCore->publish(config_classad);
}


#if HAVE_BACKFILL

void
ResMgr::backfillMgrDone()
{
	ASSERT( m_backfill_mgr );
	dprintf( D_FULLDEBUG, "BackfillMgr now ready to be deleted\n" );
	delete m_backfill_mgr;
	m_backfill_mgr = NULL;
	m_backfill_shutdown_pending = false;

		// We should call backfillConfig() again, since now that the
		// "old" manager is gone, we might want to allocate a new one
	backfillConfig();
}


static bool
verifyBackfillSystem( const char* sys )
{

#if HAVE_BOINC
	if( ! stricmp(sys, "BOINC") ) {
		return true;
	}
#endif /* HAVE_BOINC */

	return false;
}


bool
ResMgr::backfillConfig()
{
	if( m_backfill_shutdown_pending ) {
			/*
			  we're already in the middle of trying to reconfig the
			  backfill manager, anyway.  we can only get to this point
			  if we had 1 backfill system running, then we either
			  change the system we want or disable backfill entirely,
			  and while we're waiting for the old system to cleanup,
			  we get *another* reconfig.  in this case, we do NOT want
			  to act on the new reconfig until the old reconfig had a
			  chance to complete.  since we'll call backfillConfig()
			  from backfillMgrDone(), anyway, there's no harm in just
			  returning immediately at this point, and plenty of harm
			  that could come from trying to proceed. ;)
			*/
		dprintf( D_ALWAYS, "Got another reconfig while waiting for the old "
				 "backfill system to finish cleaning up, delaying\n" );
		return true;
	}

	if( ! param_boolean("ENABLE_BACKFILL", false) ) {
		if( m_backfill_mgr ) {
			dprintf( D_ALWAYS, 
					 "ENABLE_BACKFILL is false, destroying BackfillMgr\n" );
			if( m_backfill_mgr->destroy() ) {
					// nothing else to cleanup now, we can delete it 
					// immediately...
				delete m_backfill_mgr;
				m_backfill_mgr = NULL;
			} else {
					// backfill_mgr told us we have to wait, so just
					// return for now and we'll finish deleting this
					// in ResMgr::backfillMgrDone(). 
				dprintf( D_ALWAYS, "BackfillMgr still has cleanup to "
						 "perform, postponing delete\n" );
				m_backfill_shutdown_pending = true;
			}
		}
		return false;
	}

	char* new_system = param( "BACKFILL_SYSTEM" );
	if( ! new_system ) {
		dprintf( D_ALWAYS, "ERROR: ENABLE_BACKFILL is TRUE, but "	
				 "BACKFILL_SYSTEM is undefined!\n" );
		return false;
	}
	if( ! verifyBackfillSystem(new_system) ) {
		dprintf( D_ALWAYS,
				 "ERROR: BACKFILL_SYSTEM '%s' not supported, ignoring\n",
				 new_system );
		free( new_system );
		return false;
	}
		
	if( m_backfill_mgr ) {
		if( ! stricmp(new_system, m_backfill_mgr->backfillSystemName()) ) {
				// same as before
			free( new_system );
				// since it's already here and we're keeping it, tell
				// it to reconfig (if that matters)
			m_backfill_mgr->reconfig();
				// we're done 
			return true;
		} else { 
				// different!
			dprintf( D_ALWAYS, "BACKFILL_SYSTEM has changed "
					 "(old: '%s', new: '%s'), re-initializing\n",
					 m_backfill_mgr->backfillSystemName(), new_system );
			if( m_backfill_mgr->destroy() ) {
					// nothing else to cleanup now, we can delete it 
					// immediately...
				delete m_backfill_mgr;
				m_backfill_mgr = NULL;
			} else {
					// backfill_mgr told us we have to wait, so just
					// return for now and we'll finish deleting this
					// in ResMgr::backfillMgrDone(). 
				dprintf( D_ALWAYS, "BackfillMgr still has cleanup to "
						 "perform, postponing delete\n" );
				m_backfill_shutdown_pending = true;
				return true;
			}
		}
	}

		// if we got this far, it means we've got a valid system, but
		// no manager object.  so, depending on the system,
		// instantiate the right thing.
#if HAVE_BOINC
	if( ! stricmp(new_system, "BOINC") ) {
		m_backfill_mgr = new BOINC_BackfillMgr();
		if( ! m_backfill_mgr->init() ) {
			dprintf( D_ALWAYS, "ERROR initializing BOINC_BackfillMgr\n" );
			delete m_backfill_mgr;
			m_backfill_mgr = NULL;
			free( new_system );
			return false;
		}
	}
#endif /* HAVE_BOINC */

	if( ! m_backfill_mgr ) {
			// this is impossible, since we've already verified above
		EXCEPT( "IMPOSSILE: unrecognized BACKFILL_SYSTEM: '%s'",
				new_system );
	}

	dprintf( D_ALWAYS, "Created a %s Backfill Manager\n", 
			 m_backfill_mgr->backfillSystemName() );

	free( new_system );

	return true;
}



#endif /* HAVE_BACKFILL */


void
ResMgr::init_resources( void )
{
	int i, num_res;
	CpuAttributes** new_cpu_attrs;

		// These things can only be set once, at startup, so they
		// don't need to be in build_cpu_attrs() at all.
	if (param_boolean("ALLOW_VM_CRUFT", true)) {
		max_types = param_integer("MAX_SLOT_TYPES",
								  param_integer("MAX_VIRTUAL_MACHINE_TYPES",
												10));
	} else {
		max_types = param_integer("MAX_SLOT_TYPES", 10);
	}

	max_types += 1;

		// The reason this isn't on the stack is b/c of the variable
		// nature of max_types. *sigh*  
	type_strings = new StringList*[max_types];
	memset( type_strings, 0, (sizeof(StringList*) * max_types) );

		// Fill in the type_strings array with all the appropriate
		// string lists for each type definition.  This only happens
		// once!  If you change the type definitions, you must restart
		// the startd, or else too much weirdness is possible.
	initTypes( 1 );

		// First, see how many slots of each type are specified.
	num_res = countTypes( &type_nums, true );

	if( ! num_res ) {
			// We're not configured to advertise any nodes.
		resources = NULL;
		id_disp = new IdDispenser( num_cpus(), 1 );
		return;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  Since this is the startup-code
		// we'll let it EXCEPT() if there is an error.
	new_cpu_attrs = buildCpuAttrs( num_res, type_nums, true );
	if( ! new_cpu_attrs ) {
		EXCEPT( "buildCpuAttrs() failed and should have already EXCEPT'ed" );
	}

		// Now, we can finally allocate our resources array, and
		// populate it.  
	for( i=0; i<num_res; i++ ) {
		addResource( new Resource( new_cpu_attrs[i], i+1 ) );
	}

		// We can now seed our IdDispenser with the right slot id. 
	id_disp = new IdDispenser( num_cpus(), i+1 );

		// Finally, we can free up the space of the new_cpu_attrs
		// array itself, now that all the objects it was holding that
		// we still care about are stashed away in the various
		// Resource objects.  Since it's an array of pointers, this
		// won't touch the objects at all.
	delete [] new_cpu_attrs;

#if HAVE_BACKFILL
	backfillConfig();
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr = new StartdHookMgr;
	m_hook_mgr->initialize();
#endif
}

bool
ResMgr::reconfig_resources( void )
{
	int t, i, cur, num;
	CpuAttributes** new_cpu_attrs;
	int max_num = num_cpus();
	int* cur_type_index;
	Resource*** sorted_resources;	// Array of arrays of pointers.
	Resource* rip;

#if HAVE_BACKFILL
	backfillConfig();
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr->reconfig();
#endif

#if HAVE_HIBERNATION
	updateHibernateConfiguration();
#endif /* HAVE_HIBERNATION */

		// Tell each resource to reconfig itself.
	walk(&Resource::reconfig);

		// See if any new types were defined.  Don't except if there's
		// any errors, just dprintf().
	initTypes( 0 );

		// First, see how many slots of each type are specified.
	num = countTypes( &new_type_nums, false );

	if( typeNumCmp(new_type_nums, type_nums) ) {
			// We want the same number of each slot type that we've got
			// now.  We're done!
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  
	new_cpu_attrs = buildCpuAttrs( num, new_type_nums, false );
	if( ! new_cpu_attrs ) {
			// There was an error, abort.  We still return true to
			// indicate that we're done doing our thing...
		dprintf( D_ALWAYS, "Aborting slot type reconfig.\n" );
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		////////////////////////////////////////////////////
		// Sort all our resources by type and state.
		////////////////////////////////////////////////////

		// Allocate and initialize our arrays.
	sorted_resources = new Resource** [max_types];
	for( i=0; i<max_types; i++ ) {
		sorted_resources[i] = new Resource* [max_num];
		memset( sorted_resources[i], 0, (max_num*sizeof(Resource*)) ); 
	}

	cur_type_index = new int [max_types];
	memset( cur_type_index, 0, (max_types*sizeof(int)) );

		// Populate our sorted_resources array by type.
	for( i=0; i<nresources; i++ ) {
		t = resources[i]->type();
		(sorted_resources[t])[cur_type_index[t]] = resources[i];
		cur_type_index[t]++;
	}
	
		// Now, for each type, sort our resources by state.
	for( t=0; t<max_types; t++ ) {
		ASSERT( cur_type_index[t] == type_nums[t] );
		qsort( sorted_resources[t], type_nums[t], 
			   sizeof(Resource*), &claimedRankCmp );
	}

		////////////////////////////////////////////////////
		// Decide what we need to do.
		////////////////////////////////////////////////////
	cur = -1;
	for( t=0; t<max_types; t++ ) {
		for( i=0; i<new_type_nums[t]; i++ ) {
			cur++;
			if( ! (sorted_resources[t])[i] ) {
					// If there are no more existing resources of this
					// type, we'll need to allocate one.
				alloc_list.Append( new_cpu_attrs[cur] );
				continue;
			}
			if( (sorted_resources[t])[i]->type() ==
				new_cpu_attrs[cur]->type() ) {
					// We've already got a Resource for this slot, so we
					// can delete it.
				delete new_cpu_attrs[cur];
				continue;
			}
		}
			// We're done with the new slots of this type.  See if there
			// are any Resources left over that need to be destroyed.
		for( ; i<max_num; i++ ) {
			if( (sorted_resources[t])[i] ) {
				destroy_list.Append( (sorted_resources[t])[i] );
			} else {
				break;
			}
		}
	}

		////////////////////////////////////////////////////
		// Finally, act on our decisions.
		////////////////////////////////////////////////////

		// Everything we care about in new_cpu_attrs is saved
		// elsewhere, and the rest has already been deleted, so we
		// should now delete the array itself. 
	delete [] new_cpu_attrs;

		// Cleanup our memory.
	for( i=0; i<max_types; i++ ) {	
		delete [] sorted_resources[i];
	}
	delete [] sorted_resources;
	delete [] cur_type_index;

		// See if there's anything to destroy, and if so, do it. 
	destroy_list.Rewind();
	while( destroy_list.Next(rip) ) {
		rip->dprintf( D_ALWAYS,
					  "State change: resource no longer needed by configuration\n" );
		rip->set_destination_state( delete_state );
	}

		// Finally, call our helper, so that if all the slots we need to
		// get rid of are gone by now, we'll allocate the new ones. 
	return processAllocList();
}


CpuAttributes** 
ResMgr::buildCpuAttrs( int total, int* type_num_array, bool except )
{
	int i, j, num;
	CpuAttributes* cap;
	CpuAttributes** cap_array;

		// Available system resources.
	AvailAttributes avail( m_attr );
	
	cap_array = new CpuAttributes* [total];
	if( ! cap_array ) {
		EXCEPT( "Out of memory!" );
	}

	num = 0;
	for( i=0; i<max_types; i++ ) {
		if( type_num_array[i] ) {
			for( j=0; j<type_num_array[i]; j++ ) {
				cap = buildSlot( num+1, type_strings[i], i, except );
				if( avail.decrement(cap) ) {
					cap_array[num] = cap;
					num++;
				} else {
						// We ran out of system resources.  
					dprintf( D_ALWAYS, 
							 "ERROR: Can't allocate %s slot of type %d\n",
							 num_string(j+1), i );
					dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
					cap->show_totals( D_ALWAYS );
					dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
					avail.show_totals( D_ALWAYS, cap );
					delete cap;	// This isn't in our array yet.
					if( except ) {
						EXCEPT( "Ran out of system resources" );
					} else {
							// Gracefully cleanup and abort
						for( i=0; i<num; i++ ) {
							delete cap_array[i];
						}
						delete [] cap_array;
						return NULL;
					}
				}					
			}
		}
	}

		// now replace "auto" shares with final value
	for( i=0; i<num; i++ ) {
		cap = cap_array[i];
		if( !avail.computeAutoShares(cap) ) {

				// We ran out of system resources.  
			dprintf( D_ALWAYS, 
					 "ERROR: Can't allocate slot id %d during auto "
					 "allocation of resources\n", i+1 );
			dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
			cap->show_totals( D_ALWAYS );
			dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
			avail.show_totals( D_ALWAYS, cap );
			delete cap;	// This isn't in our array yet.
			if( except ) {
				EXCEPT( "Ran out of system resources in auto allocation" );
			} else {
					// Gracefully cleanup and abort
				for( i=0; i<num; i++ ) {
					delete cap_array[i];
				}
				delete [] cap_array;
				return NULL;
			}
		}					
	}

	return cap_array;
}
	
static void
_checkInvalidParam( const char* name, bool except ) {
	char* tmp;
	if ((tmp = param(name))) {
		if (except) {
			EXCEPT( "Can't define %s in the config file", name );
		} else {
			dprintf( D_ALWAYS, 
					 "Can't define %s in the config file, ignoring\n",
					 name ); 
		}
		free(tmp);
	}
}

void
ResMgr::initTypes( bool except )
{
	int i;
	char* tmp;
	MyString buf;

	_checkInvalidParam("SLOT_TYPE_0", except);
		// CRUFT
	_checkInvalidParam("VIRTUAL_MACHINE_TYPE_0", except);

	if (! type_strings[0]) {
			// Type 0 is the special type for evenly divided slots. 
		type_strings[0] = new StringList();
		type_strings[0]->initializeFromString("auto");
	}	

	for( i=1; i < max_types; i++ ) {
		if( type_strings[i] ) {
			continue;
		}
		buf.sprintf("SLOT_TYPE_%d", i);
		tmp = param(buf.Value());
		if (!tmp) {
			if (param_boolean("ALLOW_VM_CRUFT", true)) {
				buf.sprintf("VIRTUAL_MACHINE_TYPE_%d", i);
				if (!(tmp = param(buf.Value()))) {
					continue;
				}
			} else {
				continue;
			}
		}
		type_strings[i] = new StringList();
		type_strings[i]->initializeFromString( tmp );
		free( tmp );
	}
}


int
ResMgr::countTypes( int** array_ptr, bool except )
{
	int i, num=0, num_set=0;
    MyString param_name;
    MyString cruft_name;
	int* my_type_nums = new int[max_types];

	if( ! array_ptr ) {
		EXCEPT( "ResMgr:countTypes() called with NULL array_ptr!" );
	}

		// Type 0 is special, user's shouldn't define it.
	_checkInvalidParam("NUM_SLOTS_TYPE_0", except);
		// CRUFT
	_checkInvalidParam("NUM_VIRTUAL_MACHINES_TYPE_0", except);

	for( i=1; i<max_types; i++ ) {
		param_name.sprintf("NUM_SLOTS_TYPE_%d", i);
		if (param_boolean("ALLOW_VM_CRUFT", true)) {
			cruft_name.sprintf("NUM_VIRTUAL_MACHINES_TYPE_%d", i);
			my_type_nums[i] = param_integer(param_name.Value(),
											 param_integer(cruft_name.Value(),
														   0));
		} else {
			my_type_nums[i] = param_integer(param_name.Value(), 0);
		}
		if (my_type_nums[i]) {
			num_set = 1;
			num += my_type_nums[i];
		}
	}

	if( num_set ) {
			// We found type-specific stuff, use that.
		my_type_nums[0] = 0;
	} else {
			// We haven't found any special types yet.  Therefore,
			// we're evenly dividing things, so we only have to figure
			// out how many nodes to advertise.
		if (param_boolean("ALLOW_VM_CRUFT", true)) {
			my_type_nums[0] = param_integer("NUM_SLOTS",
										  param_integer("NUM_VIRTUAL_MACHINES",
														num_cpus()));
		} else {
			my_type_nums[0] = param_integer("NUM_SLOTS", num_cpus());
		}
		num = my_type_nums[0];
	}
	*array_ptr = my_type_nums;
	return num;
}


bool
ResMgr::typeNumCmp( int* a, int* b )
{
	int i;
	for( i=0; i<max_types; i++ ) {
		if( a[i] != b[i] ) {
			return false;
		}
	}
	return true; 
}


CpuAttributes*
ResMgr::buildSlot( int slot_id, StringList* list, int type, bool except )
{
	char *attr, *val;
	int cpus=0, ram=0;
	float disk=0, swap=0, share;
	float default_share = AUTO_SHARE;

	MyString execute_dir, partition_id;
	GetConfigExecuteDir( slot_id, &execute_dir, &partition_id );

	if ( list == NULL) {
	  // give everything the default share and return
	  
	  cpus = compute_cpus( default_share );
	  ram = compute_phys_mem( default_share );
	  swap = default_share;
	  disk = default_share;
	  
	  return new CpuAttributes( m_attr, type, cpus, ram, swap, disk, execute_dir, partition_id );
	}
		// For this parsing code, deal with the following example
		// string list:
		// "c=1, r=25%, d=1/4, s=25%"
		// There may be a bare number (no equals sign) that specifies
		// the default share for any items not explicitly defined.  Example:
		// "c=1, 25%"

	list->rewind();
	while( (attr = list->next()) ) {
		if( ! (val = strchr(attr, '=')) ) {
				// There's no = in this description, it must be one 
				// percentage or fraction for all attributes.
				// For example "1/4" or "25%".  So, we can just parse
				// it as a percentage and use that for everything.
			default_share = parse_value( attr, type, except );
			if( default_share <= 0 && !IS_AUTO_SHARE(default_share) ) {
				dprintf( D_ALWAYS, "ERROR: Bad description of slot type %d: ",
						 type );
				dprintf( D_ALWAYS | D_NOHEADER,  "\"%s\" is invalid.\n", attr );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tYou must specify a percentage (like \"25%%\"), " );
				dprintf( D_ALWAYS | D_NOHEADER, "a fraction (like \"1/4\"),\n" );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tor list all attributes (like \"c=1, r=25%%, s=25%%, d=25%%\").\n" );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tSee the manual for details.\n" );
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
			}
			continue;
		}

			// If we're still here, this is part of a string that
			// lists out seperate attributes and the share for each one.
		
			// Get the value for this attribute.  It'll either be a
			// percentage, or it'll be a distinct value (in which
			// case, parse_value() will return negative.
		if( ! val[1] ) {
			dprintf( D_ALWAYS, 
					 "Can't parse attribute \"%s\" in description of slot type %d\n",
					 attr, type );
			if( except ) {
				DC_Exit( 4 );
			} else {	
				return NULL;
			}
		}
		share = parse_value( &val[1], type, except );
		if( ! share ) {
				// Invalid share.
		}

			// Figure out what attribute we're dealing with.
		switch( tolower(attr[0]) ) {
		case 'c':
			cpus = compute_cpus( share );
			break;
		case 'r':
		case 'm':
			ram = compute_phys_mem( share );
			break;
		case 's':
		case 'v':
			if( share > 0 || IS_AUTO_SHARE(share) ) {
				swap = share;
			} else {
				dprintf( D_ALWAYS,
						 "You must specify a percent or fraction for swap in slot type %d\n", 
						 type ); 
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
			}
			break;
		case 'd':
			if( share > 0 || IS_AUTO_SHARE(share) ) {
				disk = share;
			} else {
				dprintf( D_ALWAYS, 
						 "You must specify a percent or fraction for disk in slot type %d\n", 
						type ); 
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
			}
			break;
		default:
			dprintf( D_ALWAYS, "Unknown attribute \"%s\" in slot type %d\n", 
					 attr, type );
			if( except ) {
				DC_Exit( 4 );
			} else {	
				return NULL;
			}
			break;
		}
	}

		// We're all done parsing the string.  Any attribute not
		// listed will get the default share.
	if( ! cpus ) {
		cpus = compute_cpus( default_share );
	}
	if( ! ram ) {
		ram = compute_phys_mem( default_share );
	}
	if( ! swap ) {
		swap = default_share;
	}
	if( ! disk ) {
		disk = default_share;
	}

		// Now create the object.
	return new CpuAttributes( m_attr, type, cpus, ram, swap, disk, execute_dir, partition_id );
}

void
ResMgr::GetConfigExecuteDir( int slot_id, MyString *execute_dir, MyString *partition_id )
{
	MyString execute_param;
	char *execute_value = NULL;
	execute_param.sprintf("SLOT%d_EXECUTE",slot_id);
	execute_value = param( execute_param.Value() );
	if( !execute_value ) {
		execute_value = param( "EXECUTE" );
	}
	if( !execute_value ) {
		EXCEPT("EXECUTE (or %s) is not defined in the configuration.",
			   execute_param.Value());
	}

#if defined(WIN32)
	int i;
		// switch delimiter char in exec path on WIN32
	for (i=0; execute_value[i]; i++) {
		if (execute_value[i] == '/') {
			execute_value[i] = '\\';
		}
	}
#endif

	*execute_dir = execute_value;
	free( execute_value );

		// Get a unique identifier for the partition containing the execute dir
	char *partition_value = NULL;
	bool partition_rc = sysapi_partition_id( execute_dir->Value(), &partition_value );
	if( !partition_rc ) {
		EXCEPT("Failed to get partition id for %s=%s\n",
			   execute_param.Value(), execute_dir->Value());
	}
	ASSERT( partition_value );
	*partition_id = partition_value;
	free(partition_value);
}

/* 
   Parse a string in one of a few formats, and return a float that
   represents the value.  Either it's "auto", or it's a fraction, like
   "1/4", or it's a percent, like "25%" (in both cases we'd return
   .25), or it's a regular value, like "64", in which case, we return
   the negative value, so that our caller knows it's an absolute
   value, not a fraction.
*/
float
ResMgr::parse_value( const char* str, int type, bool except )
{
	char *tmp, *foo = strdup( str );
	float val;
	if( stricmp(foo,"auto") == 0 || stricmp(foo,"automatic") == 0 ) {
		free( foo );
		return AUTO_SHARE;
	}
	else if( (tmp = strchr(foo, '%')) ) {
			// It's a percent
		*tmp = '\0';
		val = (float)atoi(foo) / 100;
		free( foo );
		return val;
	} else if( (tmp = strchr(foo, '/')) ) {
			// It's a fraction
		*tmp = '\0';
		if( ! tmp[1] ) {
			dprintf( D_ALWAYS, "Can't parse attribute \"%s\" in description of slot type %d\n",
					 foo, type );
			if( except ) {
				DC_Exit( 4 );
			} else {	
				free( foo );
				return 0;
			}
		}
		val = (float)atoi(foo) / ((float)atoi(&tmp[1]));
		free( foo );
		return val;
	} else {
			// This must just be an absolute value.  Return it as a negative
			// float, so the caller knows it's not a fraction.
		val = -(float)atoi(foo);
		free( foo );
		return val;
	}
}
 
 
/*
  Generally speaking, we want to round down for fractional amounts of
  a CPU.  However, we never want to advertise less than 1.  Plus, if
  the share in question is negative, it means it's an absolute value, not a
  fraction.
*/
int
ResMgr::compute_cpus( float share )
{
	int cpus;
	if( IS_AUTO_SHARE(share) ) {
			// Currently, "auto" for cpus just means 1 cpu per slot.
		return 1;
	}
	if( share > 0 ) {
		cpus = (int)floor( share * num_cpus() );
	} else {
		cpus = (int)floor( -share );
	}
	if( ! cpus ) {
		cpus = 1;
	}
	return cpus;
}


/*
  Generally speaking, we want to round down for fractional amounts of
  physical memory.  However, we never want to advertise less than 1.
  Plus, if the share in question is negative, it means it's an absolute
  value, not a fraction.
*/
int
ResMgr::compute_phys_mem( float share )
{
	int phys_mem;
	if( IS_AUTO_SHARE(share) ) {
			// This will be replaced later with an even share of whatever
			// memory is left over.
		return AUTO_MEM;
	}
	if( share > 0 ) {
		phys_mem = (int)floor( share * m_attr->phys_mem() );
	} else {
		phys_mem = (int)floor( -share );
	}
	if( ! phys_mem ) {
		phys_mem = 1;
	}
	return phys_mem;
}


void
ResMgr::walk( int(*func)(Resource*) )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		func(resources[i]);
	}
}


void
ResMgr::walk( ResourceMember memberfunc )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( VoidResourceMember memberfunc )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( ResourceMaskMember memberfunc, amask_t mask ) 
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))(mask);
	}
}


int
ResMgr::sum( ResourceMember memberfunc )
{
	if( ! resources ) {
		return 0;
	}
	int i, tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


float
ResMgr::sum( ResourceFloatMember memberfunc )
{
	if( ! resources ) {
		return 0;
	}
	int i;
	float tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


Resource*
ResMgr::res_max( ResourceMember memberfunc, int* val )
{
	if( ! resources ) {
		return NULL;
	}
	Resource* rip = NULL;
	int i, tmp, max = INT_MIN;

	for( i = 0; i < nresources; i++ ) {
		tmp = (resources[i]->*(memberfunc))();
		if( tmp > max ) {
			max = tmp;
			rip = resources[i];
		}
	}
	if( val ) {
		*val = max;
	}
	return rip;
}


void
ResMgr::resource_sort( ComparisonFunc compar )
{
	if( ! resources ) {
		return;
	}
	if( nresources > 1 ) {
		qsort( resources, nresources, sizeof(Resource*), compar );
	} 
}


// Methods to manipulate the supplemental ClassAd list
int
ResMgr::adlist_register( const char *name )
{
	return extra_ads.Register( name );
}

int
ResMgr::adlist_replace( const char *name, ClassAd *newAd, bool report_diff )
{
	if( report_diff ) {
		StringList ignore_list;
		MyString ignore = name;
		ignore += "_LastUpdate";
		ignore_list.append( ignore.Value() );
		return extra_ads.Replace( name, newAd, true, &ignore_list );
	}
	return extra_ads.Replace( name, newAd );
}

int
ResMgr::adlist_delete( const char *name )
{
	return extra_ads.Delete( name );
}

int
ResMgr::adlist_publish( ClassAd *resAd, amask_t mask )
{
	// Check the mask
	if (  ( mask & ( A_PUBLIC | A_UPDATE ) ) != ( A_PUBLIC | A_UPDATE )  ) {
		return 0;
	}

	return extra_ads.Publish( resAd );
}


bool
ResMgr::needsPolling( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->needsPolling() ) {
			return true;
		}
	}
	return false;
}


bool
ResMgr::hasAnyClaim( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->hasAnyClaim() ) {
			return true;
		}
	}
	return false;
}


Claim*
ResMgr::getClaimByPid( pid_t pid )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByPid(pid)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimById( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimById(id)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimByGlobalJobId( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(id)) ) {
			return foo;
		}
	}
	return NULL;
}

Claim *
ResMgr::getClaimByGlobalJobIdAndId( const char *job_id,
									const char *claimId)
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(job_id)) ) {
			if( foo == resources[i]->findClaimById(claimId) ) {
				return foo;	
			}
		}
	}
	return NULL;

}


Resource*
ResMgr::findRipForNewCOD( ClassAd* ad )
{
	if( ! resources ) {
		return NULL;
	}
	int requirements;
	int i;

		/*
          We always ensure that the request's Requirements, if any,
		  are met.  Other than that, we give out COD claims to
		  Resources in the following order:  

		  1) the Resource with the least # of existing COD claims (to
  		     ensure round-robin across resources
		  2) in case of a tie, the Resource in the best state (owner
   		     or unclaimed, not claimed)
		  3) in case of a tie, the Claimed resource with the lowest
  		     value of machine Rank for its claim
		*/

		// sort resources based on the above order
	resource_sort( newCODClaimCmp );

		// find the first one that matches our requirements 
	for( i = 0; i < nresources; i++ ) {
		if( ad->EvalBool( ATTR_REQUIREMENTS, resources[i]->r_classad,
						  requirements ) == 0 ) {
			requirements = 0;
		}
		if( requirements ) { 
			return resources[i];
		}
	}
	return NULL;
}



Resource*
ResMgr::get_by_cur_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
		if( resources[i]->r_pre &&
			resources[i]->r_pre->idMatches(id) ) {
			return resources[i];
		}
		if( resources[i]->r_pre_pre &&
			resources[i]->r_pre_pre->idMatches(id) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_name( char* name )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( !strcmp(resources[i]->r_name, name) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_slot_id( int id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_id == id ) {
			return resources[i];
		}
	}
	return NULL;
}


State
ResMgr::state( void )
{
	if( ! resources ) {
		return owner_state;
	}
	State s;
	Resource* rip;
	int i, is_owner = 0;
	for( i = 0; i < nresources; i++ ) {
		rip = resources[i];
			// if there are *any* COD claims at all (active or not),
			// we should say this slot is claimed so preen doesn't
			// try to clean up directories for the COD claim(s).
		if( rip->r_cod_mgr->numClaims() > 0 ) {
			return claimed_state;
		}
		s = rip->state();
		switch( s ) {
		case claimed_state:
		case preempting_state:
			return s;
			break;
		case owner_state:
			is_owner = 1;
			break;
		default:
			break;
		}
	}
	if( is_owner ) {
		return owner_state;
	} else {
		return s;
	}
}


void
ResMgr::final_update( void )
{
	if( ! resources ) {
		return;
	}
	walk( &Resource::final_update );
}


int
ResMgr::force_benchmark( void )
{
	if( ! resources ) {
		return 0;
	}
	return resources[0]->force_benchmark();
}


int
ResMgr::send_update( int cmd, ClassAd* public_ad, ClassAd* private_ad,
					 bool nonblock )
{
		// Increment the resmgr's count of updates.
	num_updates++;
		// Actually do the updates, and return the # of updates sent.
	return daemonCore->sendUpdates(cmd, public_ad, private_ad, nonblock);
}


void
ResMgr::first_eval_and_update_all( void )
{
	num_updates = 0;
	walk( &Resource::eval_and_update );
	report_updates();
	check_polling();
	check_use();
}


void
ResMgr::eval_and_update_all( void )
{
	compute( A_TIMEOUT | A_UPDATE );
	first_eval_and_update_all();
}


void
ResMgr::eval_all( void )
{
	num_updates = 0;
	compute( A_TIMEOUT );
	walk( &Resource::eval_state );
	report_updates();
	check_polling();
}


void
ResMgr::report_updates( void )
{
	if( !num_updates ) {
		return;
	}

	CollectorList* collectors = daemonCore->getCollectorList();
	if( collectors ) {
		MyString list;
		Daemon * collector;
		collectors->rewind();
		while (collectors->next (collector)) {
			list += collector->fullHostname();
			list += " ";
		}
		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n", 
				 num_updates, list.Value());
	}  
}

void
ResMgr::refresh_benchmarks()
{
	if( ! resources ) {
		return;
	}

	walk( &Resource::refresh_classad, A_TIMEOUT );
}

void
ResMgr::compute( amask_t how_much )
{
	if( ! resources ) {
		return;
	}

		// Since lots of things want to know this, just get it from
		// the kernel once and share the value...
	cur_time = time( 0 ); 

	m_attr->compute( (how_much & ~(A_SUMMED)) | A_SHARED );
	walk( &Resource::compute, (how_much & ~(A_SHARED)) );
	m_attr->compute( (how_much & ~(A_SHARED)) | A_SUMMED );
	walk( &Resource::compute, (how_much | A_SHARED) );

		// Sort the resources so when we're assigning owner load
		// average and keyboard activity, we get to them in the
		// following state order: Owner, Unclaimed, Matched, Claimed
		// Preempting 
	resource_sort( ownerStateCmp );

	assign_load();
	assign_keyboard();

	if( vmapi_is_virtual_machine() == TRUE ) {
		ClassAd* host_classad;
		vmapi_request_host_classAd();
		host_classad = vmapi_get_host_classAd();
		if( host_classad ) {
			int i;
			for( i = 0; i < nresources; i++ ) {
				if( resources[i]->r_classad )
					MergeClassAds( resources[i]->r_classad, host_classad, true );
			}
		}

	}

		// Now that everything has actually been computed, we can
		// refresh our interval classad with all the current values of
		// everything so that when we evaluate our state or any other
		// expressions, we've got accurate data to evaluate.
	walk( &Resource::refresh_classad, how_much );

		// Now that we have an updated interval classad for each
		// resource, we can "compute" anything where we need to 
		// evaluate classad expressions to get the answer.
	walk( &Resource::compute, A_EVALUATED );

		// Next, we can publish any results from that to our interval
		// classads to make sure those are still up-to-date
	walk( &Resource::refresh_classad, A_EVALUATED );

		// Finally, now that all the interval classads are up to date
		// with all the attributes they could possibly have, we can
		// publish the cross-slot attributes desired from
		// STARTD_SLOT_ATTRS into each slots's interval ClassAd.
	walk( &Resource::refresh_classad, A_SHARED_SLOT );

		// Now that we're done, we can display all the values.
	walk( &Resource::display, how_much );
}


void
ResMgr::publish( ClassAd* cp, amask_t how_much )
{
	if( IS_UPDATE(how_much) && IS_PUBLIC(how_much) ) {
		cp->Assign(ATTR_TOTAL_SLOTS, numSlots());
		if (param_boolean("ALLOW_VM_CRUFT", true)) { 
			cp->Assign(ATTR_TOTAL_VIRTUAL_MACHINES, numSlots());
		}
	}

	starter_mgr.publish( cp, how_much );
	m_vmuniverse_mgr.publish(cp, how_much);

#if HAVE_HIBERNATION
    m_hibernation_manager->publish( *cp );
#endif

}


void
ResMgr::publishSlotAttrs( ClassAd* cap )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->publishSlotAttrs( cap );
	}
}


void
ResMgr::assign_load( void )
{
	if( ! resources ) {
		return;
	}

	int i;
	float total_owner_load = m_attr->load() - m_attr->condor_load();
	if( total_owner_load < 0 ) {
		total_owner_load = 0;
	}
	if( is_smp() ) {
			// Print out the totals we already know.
		if( DebugFlags & D_LOAD ) {
			dprintf( D_FULLDEBUG, 
					 "%s %.3f\t%s %.3f\t%s %.3f\n",  
					 "SystemLoad:", m_attr->load(),
					 "TotalCondorLoad:", m_attr->condor_load(),
					 "TotalOwnerLoad:", total_owner_load );
		}

			// Initialize everything to 0.  Only need this for SMP
			// machines, since on single CPU machines, we just assign
			// all OwnerLoad to the 1 CPU.
		for( i = 0; i < nresources; i++ ) {
			resources[i]->set_owner_load( 0 );
		}
	}

		// So long as there's at least two more resources and the
		// total owner load is greater than 1.0, assign an owner load
		// of 1.0 to each CPU.  Once we get below 1.0, we assign all
		// the rest to the next CPU.  So, for non-SMP machines, we
		// never hit this code, and always assign all owner load to
		// cpu1 (since i will be initialized to 0 but we'll never
		// enter the for loop).  
	for( i = 0; i < (nresources - 1) && total_owner_load > 1; i++ ) {
		resources[i]->set_owner_load( 1.0 );
		total_owner_load -= 1.0;
	}
	resources[i]->set_owner_load( total_owner_load );
}


void
ResMgr::assign_keyboard( void )
{
	if( ! resources ) {
		return;
	}

	int i;
	time_t console = m_attr->console_idle();
	time_t keyboard = m_attr->keyboard_idle();
	time_t max;

		// First, initialize all CPUs to the max idle time we've got,
		// which is some configurable amount of minutes longer than
		// the time since we started up.
	max = (cur_time - startd_startup) + disconnected_keyboard_boost;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->r_attr->set_console( max );
		resources[i]->r_attr->set_keyboard( max );
	}

		// Now, assign console activity to all CPUs that care.
		// Notice, we should also assign keyboard here, since if
		// there's console activity, there's (by definition) keyboard 
		// activity as well.
	for( i = 0; i < console_slots  && i < nresources; i++ ) {
		resources[i]->r_attr->set_console( console );
		resources[i]->r_attr->set_keyboard( console );
	}

		// Finally, assign keyboard activity to all CPUS that care. 
	for( i = 0; i < keyboard_slots && i < nresources; i++ ) {
		resources[i]->r_attr->set_keyboard( keyboard );
	}
}


void
ResMgr::check_polling( void )
{
	if( ! resources ) {
		return;
	}

	if( needsPolling() || m_attr->condor_load() > 0 ) {
		start_poll_timer();
	} else {
		cancel_poll_timer();
	}
}


int
ResMgr::start_update_timer( void )
{
	up_tid = 
		daemonCore->Register_Timer( update_interval, update_interval,
							(TimerHandlercpp)&ResMgr::eval_and_update_all,
							"eval_and_update_all", this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
ResMgr::start_poll_timer( void )
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid = 
		daemonCore->Register_Timer( polling_interval,
							polling_interval, 
							(TimerHandlercpp)&ResMgr::eval_all,
							"poll_resources", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	dprintf( D_FULLDEBUG, "Started polling timer.\n" );
	return TRUE;
}


void
ResMgr::cancel_poll_timer( void )
{
	int rval;
	if( poll_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( poll_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel polling timer (%d): "
					 "daemonCore error\n", poll_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled polling timer (%d)\n",
					 poll_tid );
		}
		poll_tid = -1;
	}
}


void
ResMgr::reset_timers( void )
{
	if( poll_tid != -1 ) {
		daemonCore->Reset_Timer( poll_tid, polling_interval, 
								 polling_interval );
	}
	if( up_tid != -1 ) {
		daemonCore->Reset_Timer( up_tid, update_interval, 
								 update_interval );
	}

#if HAVE_HIBERNATION
	resetHibernateTimer();
#endif /* HAVE_HIBERNATION */

}


void
ResMgr::addResource( Resource *rip )
{
	Resource** new_resources = NULL;

	if( !rip ) {
		EXCEPT("Error: attempt to add a NULL resource\n");
	}

	new_resources = new Resource*[nresources + 1];
	if( !new_resources ) {
		EXCEPT("Failed to allocate memory for new resource\n");
	}

		// Copy over the old Resource pointers.  If nresources is 0
		// (b/c we used to be configured to have no slots), this won't
		// copy anything (and won't seg fault).
	memcpy( (void*)new_resources, (void*)resources, 
			(sizeof(Resource*)*nresources) );

	new_resources[nresources] = rip;


	if( resources ) {
		delete [] resources;
	}

	resources = new_resources;
	nresources++;
}


bool
ResMgr::removeResource( Resource* rip )
{
	int i, j;
	Resource** new_resources = NULL;
	Resource* rip2;

	if( nresources > 1 ) {
			// There are still more resources after this one is
			// deleted, so we'll need to make a new resources array
			// without this resource. 
		new_resources = new Resource* [ nresources - 1 ];
		j = 0;
		for( i = 0; i < nresources; i++ ) {
			if( resources[i] != rip ) {
				new_resources[j++] = resources[i];
			}
		}

		if ( j == nresources ) { // j == i would work too
				// The resource was not found, which should never happen
			delete [] new_resources;
			return false;
		}
	} 

		// Remove this rip from our destroy_list.
	destroy_list.Rewind();
	while( destroy_list.Next(rip2) ) {
		if( rip2 == rip ) {
			destroy_list.DeleteCurrent();
			break;
		}
	}

		// Now, delete the old array and start using the new one, if
		// it's there at all.  If not, it'll be NULL and this will all
		// still be what we want.
	delete [] resources;
	resources = new_resources;
	nresources--;
	
		// Return this Resource's ID to the dispenser.
		// If it is a dynamic slot it's reusing its partitionable
		// parent's id, so we don't want to free the id.
	if( Resource::DYNAMIC_SLOT != rip->get_feature() ) {
		id_disp->insert( rip->r_id );
	}

		// Tell the collector this Resource is gone.
	rip->final_update();

		// Log a message that we're going away
	rip->dprintf( D_ALWAYS, "Resource no longer needed, deleting\n" );

		// At last, we can delete the object itself.
	delete rip;

	return true;
}


void
ResMgr::deleteResource( Resource* rip )
{
	if( ! removeResource( rip ) ) {
			// Didn't find it.  This is where we'll hit if resources
			// is NULL.  We should never get here, anyway (we'll never
			// call deleteResource() if we don't have any resources. 
		EXCEPT( "ResMgr::deleteResource() failed: couldn't find resource" );
	}

		// Now that a Resource is gone, see if we're done deleting
		// Resources and see if we should allocate any. 
	if( processAllocList() ) {
			// We're done allocating, so we can finish our reconfig. 
		finish_main_config();
	}
}


void
ResMgr::makeAdList( ClassAdList *list )
{
	ClassAd* ad;
	int i;

		// Make sure everything is current
	compute( A_TIMEOUT | A_UPDATE );

		// We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
		// collector normally does this, so if we're servicing a
		// QUERY_STARTD_ADS commannd, we need to do this ourselves or
		// some timing stuff won't work. 
	for( i=0; i<nresources; i++ ) {
		ad = new ClassAd;
		resources[i]->publish( ad, A_ALL_PUB ); 
		ad->Assign( ATTR_LAST_HEARD_FROM, (int)cur_time );
		list->Insert( ad );
	}
}


bool
ResMgr::processAllocList( void )
{
	if( ! destroy_list.IsEmpty() ) {
			// Can't start allocating until everything has been
			// destroyed.
		return false;
	}
	if( alloc_list.IsEmpty() ) {
		return true;  // Since there's nothing to allocate...
	}

		// We're done destroying, and there's something to allocate.  

		// Create the new Resource objects.
	CpuAttributes* cap;
	alloc_list.Rewind();
	while( alloc_list.Next(cap) ) {
		addResource( new Resource( cap, nextId() ) );
		alloc_list.DeleteCurrent();
	}	

	delete [] type_nums;
	type_nums = new_type_nums;
	new_type_nums = NULL;

	return true; 	// Since we're done allocating.
}


#if HAVE_HIBERNATION

HibernationManager const& ResMgr::getHibernationManager(void) const
{
	return *m_hibernation_manager;
}


void ResMgr::updateHibernateConfiguration() {
	m_hibernation_manager->update();
	if ( m_hibernation_manager->wantsHibernate() ) {
		if ( -1 == m_hibernate_tid ) {
			startHibernateTimer();
		}
	} else {
		if ( -1 != m_hibernate_tid ) {
			cancelHibernateTimer();
		}
	}
}


int
ResMgr::allHibernating( MyString &target ) const
{
    	// fail if there is no resource or if we are
		// configured not to hibernate
	if (   !resources  ||  !m_hibernation_manager->wantsHibernate()  ) {
		dprintf( D_FULLDEBUG, "allHibernating: doesn't want hibernate\n" );
		return 0;
	}

		// The following may evaluate to true even if there
		// is a claim on one or more of the resources, so we
		// don't bother checking for claims first. 
		// 
		// We take largest value as the representative 
		// hibernation level for this machine
	target = "";
	MyString str;
	int level = 0;
	bool activity = false;
	for( int i = 0; i < nresources; i++ ) {
		
		str = "";
		if ( !resources[i]->evaluateHibernate ( str ) ) {
			return 0;
		}

		int tmp = m_hibernation_manager->stringToSleepState (
			str.Value () );
		
		dprintf ( D_FULLDEBUG, 
			"allHibernating: resource #%d: '%s' (0x%x)\n",
			i + 1, str.Value (), tmp );
		
		if ( 0 == tmp ) {
			activity = true;
		}

		if ( tmp > level ) {
			target = str;
			level = tmp;
		}
	}
	return activity ? 0 : level;
}


void 
ResMgr::checkHibernate( void )
{

		// If all resources have gone unused for some time	
		// then put the machine to sleep
	MyString	target;
	int level = allHibernating( target );
	if( level > 0 ) {

        if( !m_hibernation_manager->canHibernate() ) {
            dprintf ( D_ALWAYS, "ResMgr: ERROR: Ignoring "
                "HIBERNATE: Machine does not support any "
                "sleep states.\n" );
            return;
        }

        if( !m_hibernation_manager->canWake() ) {
			NetworkAdapterBase	*netif =
				m_hibernation_manager->getNetworkAdapter();
            dprintf ( D_ALWAYS, "ResMgr: ERROR: Ignoring "
					  "HIBERNATE: Machine cannot be woken by its "
					  "public network adapter (%s).\n",
					  netif->interfaceName() );
            return;
		}        

		dprintf ( D_ALWAYS, "ResMgr: This machine is about to "
        		"enter hibernation\n" );

        //
		// Set the hibernation state, shutdown the machine's slot 
	    // and hibernate the machine. We turn off the local slots
	    // so the StartD will remove any jobs that are currently 
	    // running as well as stop accepting new ones, since--on 
	    // Windows anyway--there is the possibility that a job
	    // may be matched to this machine between the time it
	    // is told hibernate and the time it actually does.
		//
	    // Setting the state here also ensures the Green Computing
	    // plug-in will know the this ad belongs to it when the
	    // Collector invalidates it.
	    //
		if ( disableResources( target ) ) {
			m_hibernation_manager->switchToTargetState( );
		}
#     if !defined( WIN32 )
		sleep(10);
        m_hibernation_manager->setTargetState ( HibernatorBase::NONE );
        for ( int i = 0; i < nresources; ++i ) {
            resources[i]->enable();
            resources[i]->update();
	    }
		
#     endif
    }
}


int
ResMgr::startHibernateTimer( void )
{
	int interval = m_hibernation_manager->getCheckInterval();
	m_hibernate_tid = daemonCore->Register_Timer( 
		interval, interval,
		(TimerHandlercpp)&ResMgr::checkHibernate,
		"ResMgr::startHibernateTimer()", this );
	if( m_hibernate_tid < 0 ) {
		EXCEPT( "Can't register hibernation timer" );
	}
	dprintf( D_FULLDEBUG, "Started hibernation timer.\n" );
	return TRUE;
}


void
ResMgr::resetHibernateTimer( void )
{
	if ( m_hibernation_manager->wantsHibernate() ) {
		if( m_hibernate_tid != -1 ) {
			int interval = m_hibernation_manager->getCheckInterval();
			daemonCore->Reset_Timer( 
				m_hibernate_tid, 
				interval, interval );
		}
	}
}


void
ResMgr::cancelHibernateTimer( void )
{
	int rval;
	if( m_hibernate_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( m_hibernate_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel hibernation timer (%d): "
				"daemonCore error\n", m_hibernate_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled hibernation timer (%d)\n",
				m_hibernate_tid );
		}
		m_hibernate_tid = -1;
	}
}


#endif /* HAVE_HIBERNATION */


void
ResMgr::check_use( void ) 
{
	int current_time = time(NULL);
	if( hasAnyClaim() ) {
		last_in_use = current_time;
	}
	if( ! startd_noclaim_shutdown ) {
			// Nothing to do.
		return;
	}
	if( current_time - last_in_use > startd_noclaim_shutdown ) {
			// We've been unused for too long, send a SIGTERM to our
			// parent, the condor_master. 
		dprintf( D_ALWAYS, 
				 "No resources have been claimed for %d seconds\n", 
				 startd_noclaim_shutdown );
		dprintf( D_ALWAYS, "Shutting down Condor on this machine.\n" );
		daemonCore->Send_Signal( daemonCore->getppid(), SIGTERM );
	}
}


int
ownerStateCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);
		// Since the State enum is already in the "right" order for
		// this kind of sort, we don't need to do anything fancy, we
		// just cast the state enum to an int and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	} 
	return 0;
}


// This is basically the same as above, except we want it in exactly
// the opposite order, so reverse the signs.
int
claimedRankCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);

	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val2 - val1;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval2 - fval1);
		return diff;
	} 
	return 0;
}


/*
  Sort resource so their in the right order to give out a new COD
  Claim.  We give out COD claims in the following order:  
  1) the Resource with the least # of existing COD claims (to ensure
     round-robin across resources
  2) in case of a tie, the Resource in the best state (owner or
     unclaimed, not claimed)
  3) in case of a tie, the Claimed resource with the lowest value of
     machine Rank for its claim
*/
int
newCODClaimCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	int numCOD1, numCOD2;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);

	numCOD1 = rip1->r_cod_mgr->numClaims();
	numCOD2 = rip2->r_cod_mgr->numClaims();

		// In the first case, sort based on # of COD claims
	diff = numCOD1 - numCOD2;
	if( diff ) {
		return diff;
	}

		// If we're still here, we've got same # of COD claims, so
		// sort based on State.  Since the state enum is already in
		// the "right" order for this kind of sort, we don't need to
		// do anything fancy, we just cast the state enum to an int
		// and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}

		// We're still here, means we've got the same number of COD
		// claims and the same state.  If that state is "Claimed" or
		// "Preempting", we want to break ties w/ the Rank expression,
		// else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	} 
	return 0;
}


void
ResMgr::FillExecuteDirsList( class StringList *list )
{
	ASSERT( list );
	for( int i=0; i<nresources; i++ ) {
		if( resources[i] ) {
			char const *execute_dir = resources[i]->executeDir();
			if( !list->contains( execute_dir ) ) {
				list->append(execute_dir);
			}
		}
	}
}

#if HAVE_HIBERNATION

int
ResMgr::disableResources( const MyString &state_str )
{

    int i; /* stupid VC6 */

	/* set the sleep state so the plugin will pickup on the
	fact that we are sleeping */
	m_hibernation_manager->setTargetState ( state_str.Value() );

    /* disable all resource on this machine */
    for ( i = 0; i < nresources; ++i ) {
        resources[i]->disable();
	}

    /* update the CM */
    bool ok = true;
    for ( i = 0; i < nresources && ok; ++i ) {
        ok = resources[i]->update_with_ack();
	}

    /* if any of the updates failed, then renable all the
    resources and try again later (next time HIBERNATE evaluates
    to an value>0) */
    if ( !ok ) {
        m_hibernation_manager->setTargetState (
            HibernatorBase::NONE );
        for ( i = 0; i < nresources; ++i ) {
            resources[i]->enable();
            resources[i]->update();
	    }
    }

    return ok;
}

#endif
