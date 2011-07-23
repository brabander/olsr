/*
 * common_types.h
 *
 *  Created on: Mar 2, 2011
 *      Author: rogge
 */

#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_

/*
 * This include file creates stdint/stdbool datatypes for
 * visual studio, because microsoft does not support C99
 */

/* support EXPORT macro of OLSR */
#ifndef EXPORT
#  define EXPORT(x) x
#endif

/* types */
#ifdef _MSC_VER
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#else
#include <inttypes.h>
#endif

#if (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L) || defined __GNUC__
#include <stdbool.h>
#endif

/* add some safe-gaurds */
#ifndef _MSC_VER
#if !defined bool || !defined true || !defined false || !defined __bool_true_false_are_defined
#error You have no C99-like boolean types. Please extend src/olsr_type.h!
#endif
#endif

#endif /* COMMON_TYPES_H_ */
