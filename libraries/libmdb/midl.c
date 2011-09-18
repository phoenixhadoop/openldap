/**	@file midl.c
 *	@brief ldap bdb back-end ID List functions */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2000-2011 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include "midl.h"

/** @defgroup internal	MDB Internals
 *	@{
 */
/** @defgroup idls	ID List Management
 *	@{
 */
#define CMP(x,y)	 ( (x) < (y) ? -1 : (x) > (y) )

#if 0	/* superseded by append/sort */
static unsigned mdb_midl_search( IDL ids, ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 0;
	int val = 0;
	unsigned n = ids[0];

	while( 0 < n ) {
		int pivot = n >> 1;
		cursor = base + pivot;
		val = CMP( ids[cursor + 1], id );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor + 1;
			n -= pivot + 1;

		} else {
			return cursor + 1;
		}
	}
	
	if( val > 0 ) {
		return cursor + 2;
	} else {
		return cursor + 1;
	}
}

int mdb_midl_insert( IDL ids, ID id )
{
	unsigned x, i;

	if (MDB_IDL_IS_RANGE( ids )) {
		/* if already in range, treat as a dup */
		if (id >= MDB_IDL_FIRST(ids) && id <= MDB_IDL_LAST(ids))
			return -1;
		if (id < MDB_IDL_FIRST(ids))
			ids[1] = id;
		else if (id > MDB_IDL_LAST(ids))
			ids[2] = id;
		return 0;
	}

	x = mdb_midl_search( ids, id );
	assert( x > 0 );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0] && ids[x] == id ) {
		/* duplicate */
		assert(0);
		return -1;
	}

	if ( ++ids[0] >= MDB_IDL_DB_MAX ) {
		if( id < ids[1] ) {
			ids[1] = id;
			ids[2] = ids[ids[0]-1];
		} else if ( ids[ids[0]-1] < id ) {
			ids[2] = id;
		} else {
			ids[2] = ids[ids[0]-1];
		}
		ids[0] = NOID;
	
	} else {
		/* insert id */
		for (i=ids[0]; i>x; i--)
			ids[i] = ids[i-1];
		ids[x] = id;
	}

	return 0;
}
#endif

IDL mdb_midl_alloc()
{
	IDL ids = malloc((MDB_IDL_UM_MAX+1) * sizeof(ID));
	*ids++ = MDB_IDL_UM_MAX;
	return ids;
}

void mdb_midl_free(IDL ids)
{
	free(ids-1);
}

int mdb_midl_shrink( IDL *idp )
{
	IDL ids = *idp;
	if (ids[-1] > MDB_IDL_UM_MAX) {
		ids = realloc(ids, (MDB_IDL_UM_MAX+1) * sizeof(ID));
		*ids++ = MDB_IDL_UM_MAX;
		*idp = ids;
		return 1;
	}
	return 0;
}

int mdb_midl_append( IDL *idp, ID id )
{
	IDL ids = *idp;
	/* Too big? */
	if (ids[0] >= ids[-1]) {
		IDL idn = ids-1;
		/* grow it */
		idn = realloc(idn, (*idn + MDB_IDL_UM_MAX + 1) * sizeof(ID));
		if (!idn)
			return -1;
		*idn++ += MDB_IDL_UM_MAX;
		ids = idn;
		*idp = ids;
	}
	ids[0]++;
	ids[ids[0]] = id;
	return 0;
}

/* Quicksort + Insertion sort for small arrays */

#define SMALL	8
#define	SWAP(a,b)	{ itmp=(a); (a)=(b); (b)=itmp; }

void
mdb_midl_sort( IDL ids )
{
	/* Max possible depth of int-indexed tree * 2 items/level */
	int istack[sizeof(int)*CHAR_BIT * 2];
	int i,j,k,l,ir,jstack;
	ID a, itmp;

	ir = ids[0];
	l = 1;
	jstack = 0;
	for(;;) {
		if (ir - l < SMALL) {	/* Insertion sort */
			for (j=l+1;j<=ir;j++) {
				a = ids[j];
				for (i=j-1;i>=1;i--) {
					if (ids[i] >= a) break;
					ids[i+1] = ids[i];
				}
				ids[i+1] = a;
			}
			if (jstack == 0) break;
			ir = istack[jstack--];
			l = istack[jstack--];
		} else {
			k = (l + ir) >> 1;	/* Choose median of left, center, right */
			SWAP(ids[k], ids[l+1]);
			if (ids[l] < ids[ir]) {
				SWAP(ids[l], ids[ir]);
			}
			if (ids[l+1] < ids[ir]) {
				SWAP(ids[l+1], ids[ir]);
			}
			if (ids[l] < ids[l+1]) {
				SWAP(ids[l], ids[l+1]);
			}
			i = l+1;
			j = ir;
			a = ids[l+1];
			for(;;) {
				do i++; while(ids[i] > a);
				do j--; while(ids[j] < a);
				if (j < i) break;
				SWAP(ids[i],ids[j]);
			}
			ids[l+1] = ids[j];
			ids[j] = a;
			jstack += 2;
			if (ir-i+1 >= j-1) {
				istack[jstack] = ir;
				istack[jstack-1] = i;
				ir = j-1;
			} else {
				istack[jstack] = j-1;
				istack[jstack-1] = l;
				l = i;
			}
		}
	}
}

unsigned mdb_mid2l_search( ID2L ids, ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 0;
	int val = 0;
	unsigned n = ids[0].mid;

	while( 0 < n ) {
		int pivot = n >> 1;
		cursor = base + pivot;
		val = CMP( id, ids[cursor + 1].mid );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor + 1;
			n -= pivot + 1;

		} else {
			return cursor + 1;
		}
	}

	if( val > 0 ) {
		return cursor + 2;
	} else {
		return cursor + 1;
	}
}

int mdb_mid2l_insert( ID2L ids, ID2 *id )
{
	unsigned x, i;

	x = mdb_mid2l_search( ids, id->mid );
	assert( x > 0 );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0].mid && ids[x].mid == id->mid ) {
		/* duplicate */
		return -1;
	}

	if ( ids[0].mid >= MDB_IDL_UM_MAX ) {
		/* too big */
		return -2;

	} else {
		/* insert id */
		ids[0].mid++;
		for (i=ids[0].mid; i>x; i--)
			ids[i] = ids[i-1];
		ids[x] = *id;
	}

	return 0;
}
/** @} */
/** @} */
