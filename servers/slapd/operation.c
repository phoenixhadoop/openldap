/* operation.c - routines to deal with pending ldap operations */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"


void
slap_op_free( Operation *op )
{
	if ( op->o_ber != NULL )
		ber_free( op->o_ber, 1 );
	if ( op->o_dn != NULL ) {
		free( op->o_dn );
	}
	if ( op->o_ndn != NULL ) {
		free( op->o_ndn );
	}
	/* ldap_pvt_thread_mutex_destroy( &op->o_abandonmutex ); */
	free( (char *) op );
}

Operation *
slap_op_add(
    Operation		**olist,
    BerElement		*ber,
    unsigned long	msgid,
    unsigned long	tag,
    char			*dn,
    int				id,
    int				connid
)
{
	Operation	**tmp;

	for ( tmp = olist; *tmp != NULL; tmp = &(*tmp)->o_next )
		;	/* NULL */

	*tmp = (Operation *) calloc( 1, sizeof(Operation) );
	ldap_pvt_thread_mutex_init( &(*tmp)->o_abandonmutex );
	(*tmp)->o_ber = ber;
	(*tmp)->o_msgid = msgid;
	(*tmp)->o_tag = tag;
	(*tmp)->o_abandon = 0;

	(*tmp)->o_dn = ch_strdup( dn != NULL ? dn : "" );
	(*tmp)->o_ndn = dn_normalize_case( ch_strdup( (*tmp)->o_dn ) );

	ldap_pvt_thread_mutex_lock( &currenttime_mutex );
	(*tmp)->o_time = currenttime;
	ldap_pvt_thread_mutex_unlock( &currenttime_mutex );
	(*tmp)->o_opid = id;
	(*tmp)->o_connid = connid;
	(*tmp)->o_next = NULL;

	return( *tmp );
}

void
slap_op_delete( Operation **olist, Operation *op )
{
	Operation	**tmp;

	for ( tmp = olist; *tmp != NULL && *tmp != op; tmp = &(*tmp)->o_next )
		;	/* NULL */

	if ( *tmp == NULL ) {
		Debug( LDAP_DEBUG_ANY, "op_delete: can't find op %ld\n",
		    op->o_msgid, 0, 0 );
		return; 
	}

	*tmp = (*tmp)->o_next;
	slap_op_free( op );
}
