#ifndef _EMUTYPES_H_
#define _EMUTYPES_H_

#ifndef INLINE

#if defined(_MSC_VER)
//#define INLINE __forceinline
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#elif defined(_MWERKS_)
#define INLINE inline
#else
#define INLINE
#endif

#endif

typedef unsigned int e_uint;
typedef signed int e_int;

typedef unsigned char e_uint8 ;
typedef signed char e_int8 ;

typedef unsigned short e_uint16 ;
typedef signed short e_int16 ;

typedef unsigned int e_uint32 ;
typedef signed int e_int32 ;


typedef e_uint8 uint8_t;
typedef e_int8 int8_t;
typedef e_uint16 uint16_t;
typedef e_int16 int16_t;
typedef e_uint32 uint32_t;
typedef e_int32 int32_t;
#ifdef _MSC_VER
typedef unsigned __int64					uint64_t;
typedef signed __int64						int64_t;
#else
__extension__ typedef unsigned long long	uint64_t;
__extension__ typedef signed long long		int64_t;
#endif

#endif
