/* sent.c - deal with data sent subsystem */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2001-2005 The OpenLDAP Foundation.
 * Portions Copyright 2001-2003 Pierangelo Masarati.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by Pierangelo Masarati for inclusion
 * in OpenLDAP Software.
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "slap.h"
#include "back-monitor.h"

enum {
	MONITOR_SENT_BYTES = 0,
	MONITOR_SENT_PDU,
	MONITOR_SENT_ENTRIES,
	MONITOR_SENT_REFERRALS,

	MONITOR_SENT_LAST
};

struct monitor_sent_t {
	struct berval	rdn;
	struct berval	nrdn;
} monitor_sent[] = {
	{ BER_BVC("cn=Bytes"),		BER_BVNULL },
	{ BER_BVC("cn=PDU"),		BER_BVNULL },
	{ BER_BVC("cn=Entries"),	BER_BVNULL },
	{ BER_BVC("cn=Referrals"),	BER_BVNULL },
	{ BER_BVNULL,			BER_BVNULL }
};

int
monitor_subsys_sent_init(
	BackendDB		*be,
	monitor_subsys_t	*ms
)
{
	monitor_info_t	*mi;
	
	Entry		**ep, *e_sent;
	monitor_entry_t	*mp;
	int			i;

	assert( be != NULL );

	mi = ( monitor_info_t * )be->be_private;

	if ( monitor_cache_get( mi, &ms->mss_ndn, &e_sent ) ) {
		Debug( LDAP_DEBUG_ANY,
			"monitor_subsys_sent_init: "
			"unable to get entry \"%s\"\n",
			ms->mss_ndn.bv_val, 0, 0 );
		return( -1 );
	}

	mp = ( monitor_entry_t * )e_sent->e_private;
	mp->mp_children = NULL;
	ep = &mp->mp_children;

	for ( i = 0; i < MONITOR_SENT_LAST; i++ ) {
		char			buf[ BACKMONITOR_BUFSIZE ];
		struct berval		nrdn, bv;
		Entry			*e;

		snprintf( buf, sizeof( buf ),
				"dn: %s,%s\n"
				"objectClass: %s\n"
				"structuralObjectClass: %s\n"
				"cn: %s\n"
				"creatorsName: %s\n"
				"modifiersName: %s\n"
				"createTimestamp: %s\n"
				"modifyTimestamp: %s\n",
				monitor_sent[ i ].rdn.bv_val,
				ms->mss_dn.bv_val,
				mi->mi_oc_monitorCounterObject->soc_cname.bv_val,
				mi->mi_oc_monitorCounterObject->soc_cname.bv_val,
				&monitor_sent[ i ].rdn.bv_val[ STRLENOF( "cn=" ) ],
				mi->mi_creatorsName.bv_val,
				mi->mi_creatorsName.bv_val,
				mi->mi_startTime.bv_val,
				mi->mi_startTime.bv_val );

		e = str2entry( buf );
		if ( e == NULL ) {
			Debug( LDAP_DEBUG_ANY,
				"monitor_subsys_sent_init: "
				"unable to create entry \"%s,%s\"\n",
				monitor_sent[ i ].rdn.bv_val,
				ms->mss_ndn.bv_val, 0 );
			return( -1 );
		}

		/* steal normalized RDN */
		dnRdn( &e->e_nname, &nrdn );
		ber_dupbv( &monitor_sent[ i ].nrdn, &nrdn );
	
		BER_BVSTR( &bv, "0" );
		attr_merge_one( e, mi->mi_ad_monitorCounter, &bv, NULL );
	
		mp = monitor_entrypriv_create();
		if ( mp == NULL ) {
			return -1;
		}
		e->e_private = ( void * )mp;
		mp->mp_info = ms;
		mp->mp_flags = ms->mss_flags \
			| MONITOR_F_SUB | MONITOR_F_PERSISTENT;

		if ( monitor_cache_add( mi, e ) ) {
			Debug( LDAP_DEBUG_ANY,
				"monitor_subsys_sent_init: "
				"unable to add entry \"%s,%s\"\n",
				monitor_sent[ i ].rdn.bv_val,
				ms->mss_ndn.bv_val, 0 );
			return( -1 );
		}
	
		*ep = e;
		ep = &mp->mp_next;
	}

	monitor_cache_release( mi, e_sent );

	return( 0 );
}

int
monitor_subsys_sent_update(
	Operation		*op,
	Entry                   *e
)
{
	monitor_info_t	*mi = ( monitor_info_t *)op->o_bd->be_private;
	
	struct berval		nrdn;
	ldap_pvt_mp_t		n;
	Attribute		*a;
	int			i;

	assert( mi );
	assert( e );

	dnRdn( &e->e_nname, &nrdn );

	for ( i = 0; i < MONITOR_SENT_LAST; i++ ) {
		if ( dn_match( &nrdn, &monitor_sent[ i ].nrdn ) ) {
			break;
		}
	}

	if ( i == MONITOR_SENT_LAST ) {
		return 0;
	}

	ldap_pvt_thread_mutex_lock(&slap_counters.sc_sent_mutex);
	switch ( i ) {
	case MONITOR_SENT_ENTRIES:
		ldap_pvt_mp_init_set( n, slap_counters.sc_entries );
		break;

	case MONITOR_SENT_REFERRALS:
		ldap_pvt_mp_init_set( n, slap_counters.sc_refs );
		break;

	case MONITOR_SENT_PDU:
		ldap_pvt_mp_init_set( n, slap_counters.sc_pdu );
		break;

	case MONITOR_SENT_BYTES:
		ldap_pvt_mp_init_set( n, slap_counters.sc_bytes );
		break;

	default:
		assert(0);
	}
	ldap_pvt_thread_mutex_unlock(&slap_counters.sc_sent_mutex);
	
	a = attr_find( e->e_attrs, mi->mi_ad_monitorCounter );
	assert( a );

	/* NOTE: no minus sign is allowed in the counters... */
	UI2BV( &a->a_vals[ 0 ], n );
	ldap_pvt_mp_clear( n );

	/* FIXME: touch modifyTimestamp? */

	return 0;
}

