/*
 * debug.h
 *
 *  Created on: 2015年6月8日
 *      Author: root
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <errno.h>

#define CLEAR( x ) memset( &( x ), 0, sizeof( x ) )

#define MIN( x, y )			( ( x ) < ( y ) ? ( x ) : ( y ) )
#define MAX( x, y )			( ( x ) > ( y ) ? ( x ) : ( y ) )
#define CLAMP( x, l, h )	( ( x ) < ( l ) ? ( l ) : ( ( x ) > ( h ) ? ( h ) : ( x ) ) )
#define ERRSTR strerror( errno )

#define LOG( ... ) fprintf( stderr, __VA_ARGS__ )

#define ERR( ... )			__info( "Error", __FILE__, __LINE__, __VA_ARGS__ )
#define ERR_ON( cond, ... ) ( ( cond ) ? ERR( __VA_ARGS__ ) : 0 )

#define CRIT( ... ) \
    do { \
		__info( "Critical", __FILE__, __LINE__, __VA_ARGS__ ); \
		exit( EXIT_FAILURE ); \
	} while( 0 )
#define CRIT_ON( cond, ... ) do { if( cond ){ CRIT( __VA_ARGS__ ); } } while( 0 )

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
static inline int __info(const char *prefix, const char *file, int line,
		const char *fmt, ...) {
	int errsv = errno;
	va_list va;

	va_start(va, fmt);
	fprintf( stderr, "%s(%s:%d): ", prefix, file, line);
	vfprintf( stderr, fmt, va);
	va_end(va);
	errno = errsv;

	return 1;
}



#endif /* DEBUG_H_ */
