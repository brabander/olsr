/* A Bison parser, made by GNU Bison 1.875a.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TOK_OPEN = 258,
     TOK_CLOSE = 259,
     TOK_SEMI = 260,
     TOK_STRING = 261,
     TOK_INTEGER = 262,
     TOK_FLOAT = 263,
     TOK_BOOLEAN = 264,
     TOK_IP6TYPE = 265,
     TOK_DEBUGLEVEL = 266,
     TOK_IPVERSION = 267,
     TOK_HNA4 = 268,
     TOK_HNA6 = 269,
     TOK_PLUGIN = 270,
     TOK_INTERFACES = 271,
     TOK_IFSETUP = 272,
     TOK_NOINT = 273,
     TOK_TOS = 274,
     TOK_WILLINGNESS = 275,
     TOK_IPCCON = 276,
     TOK_USEHYST = 277,
     TOK_HYSTSCALE = 278,
     TOK_HYSTUPPER = 279,
     TOK_HYSTLOWER = 280,
     TOK_POLLRATE = 281,
     TOK_TCREDUNDANCY = 282,
     TOK_MPRCOVERAGE = 283,
     TOK_PLNAME = 284,
     TOK_PLPARAM = 285,
     TOK_IP4BROADCAST = 286,
     TOK_IP6ADDRTYPE = 287,
     TOK_IP6MULTISITE = 288,
     TOK_IP6MULTIGLOBAL = 289,
     TOK_HELLOINT = 290,
     TOK_HELLOVAL = 291,
     TOK_TCINT = 292,
     TOK_TCVAL = 293,
     TOK_MIDINT = 294,
     TOK_MIDVAL = 295,
     TOK_HNAINT = 296,
     TOK_HNAVAL = 297,
     TOK_IP4_ADDR = 298,
     TOK_IP6_ADDR = 299,
     TOK_COMMENT = 300
   };
#endif
#define TOK_OPEN 258
#define TOK_CLOSE 259
#define TOK_SEMI 260
#define TOK_STRING 261
#define TOK_INTEGER 262
#define TOK_FLOAT 263
#define TOK_BOOLEAN 264
#define TOK_IP6TYPE 265
#define TOK_DEBUGLEVEL 266
#define TOK_IPVERSION 267
#define TOK_HNA4 268
#define TOK_HNA6 269
#define TOK_PLUGIN 270
#define TOK_INTERFACES 271
#define TOK_IFSETUP 272
#define TOK_NOINT 273
#define TOK_TOS 274
#define TOK_WILLINGNESS 275
#define TOK_IPCCON 276
#define TOK_USEHYST 277
#define TOK_HYSTSCALE 278
#define TOK_HYSTUPPER 279
#define TOK_HYSTLOWER 280
#define TOK_POLLRATE 281
#define TOK_TCREDUNDANCY 282
#define TOK_MPRCOVERAGE 283
#define TOK_PLNAME 284
#define TOK_PLPARAM 285
#define TOK_IP4BROADCAST 286
#define TOK_IP6ADDRTYPE 287
#define TOK_IP6MULTISITE 288
#define TOK_IP6MULTIGLOBAL 289
#define TOK_HELLOINT 290
#define TOK_HELLOVAL 291
#define TOK_TCINT 292
#define TOK_TCVAL 293
#define TOK_MIDINT 294
#define TOK_MIDVAL 295
#define TOK_HNAINT 296
#define TOK_HNAVAL 297
#define TOK_IP4_ADDR 298
#define TOK_IP6_ADDR 299
#define TOK_COMMENT 300




/* Copy the first part of user declarations.  */
#line 1 "oparse.y"


/*
 * OLSR ad-hoc routing table management protocol config parser
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: oparse.c,v 1.1 2004/10/16 23:22:09 kattemat Exp $
 *
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "olsrd_conf.h"

#define PARSER_DEBUG 0

#define YYSTYPE struct conf_token *

void yyerror(char *);
int yylex(void);
 


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 224 "oparse.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   84

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  46
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  49
/* YYNRULES -- Number of rules. */
#define YYNRULES  88
/* YYNRULES -- Number of states. */
#define YYNSTATES  131

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   300

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    12,    14,    16,    18,
      20,    22,    24,    26,    28,    30,    32,    34,    36,    38,
      41,    44,    47,    50,    53,    57,    58,    61,    65,    66,
      69,    73,    74,    77,    79,    81,    85,    86,    89,    91,
      93,    95,    97,    99,   101,   103,   105,   107,   109,   111,
     113,   115,   119,   120,   123,   125,   127,   129,   132,   135,
     138,   141,   144,   147,   150,   153,   156,   159,   162,   165,
     168,   171,   174,   177,   180,   183,   186,   189,   192,   195,
     198,   201,   204,   207,   210,   213,   216,   219,   223
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      47,     0,    -1,    -1,    47,    49,    -1,    47,    48,    -1,
      76,    -1,    77,    -1,    81,    -1,    82,    -1,    83,    -1,
      84,    -1,    85,    -1,    86,    -1,    87,    -1,    88,    -1,
      89,    -1,    90,    -1,    91,    -1,    94,    -1,    13,    50,
      -1,    14,    52,    -1,    16,    54,    -1,    15,    60,    -1,
      63,    57,    -1,     3,    51,     4,    -1,    -1,    51,    78,
      -1,     3,    53,     4,    -1,    -1,    53,    79,    -1,     3,
      55,     4,    -1,    -1,    55,    56,    -1,    80,    -1,    94,
      -1,     3,    58,     4,    -1,    -1,    58,    59,    -1,    94,
      -1,    64,    -1,    65,    -1,    66,    -1,    67,    -1,    68,
      -1,    69,    -1,    70,    -1,    71,    -1,    72,    -1,    73,
      -1,    74,    -1,    75,    -1,     3,    61,     4,    -1,    -1,
      61,    62,    -1,    92,    -1,    93,    -1,    94,    -1,    17,
       6,    -1,    31,    43,    -1,    32,    10,    -1,    33,    44,
      -1,    34,    44,    -1,    35,     8,    -1,    36,     8,    -1,
      37,     8,    -1,    38,     8,    -1,    39,     8,    -1,    40,
       8,    -1,    41,     8,    -1,    42,     8,    -1,    11,     7,
      -1,    12,     7,    -1,    43,    43,    -1,    44,     7,    -1,
       6,     6,    -1,    18,     9,    -1,    19,     7,    -1,    20,
       7,    -1,    21,     9,    -1,    22,     9,    -1,    23,     8,
      -1,    24,     8,    -1,    25,     8,    -1,    26,     8,    -1,
      27,     7,    -1,    28,     7,    -1,    29,     6,    -1,    30,
       6,     6,    -1,    45,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   100,   100,   101,   102,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   121,
     122,   123,   124,   125,   128,   131,   131,   134,   137,   137,
     140,   143,   143,   146,   147,   150,   153,   153,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   171,   174,   174,   177,   178,   179,   185,   208,   227,
     235,   255,   273,   280,   287,   294,   301,   308,   315,   322,
     331,   349,   363,   399,   436,   460,   467,   482,   497,   513,
     529,   538,   547,   555,   565,   581,   597,   619,   631
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TOK_OPEN", "TOK_CLOSE", "TOK_SEMI", 
  "TOK_STRING", "TOK_INTEGER", "TOK_FLOAT", "TOK_BOOLEAN", "TOK_IP6TYPE", 
  "TOK_DEBUGLEVEL", "TOK_IPVERSION", "TOK_HNA4", "TOK_HNA6", "TOK_PLUGIN", 
  "TOK_INTERFACES", "TOK_IFSETUP", "TOK_NOINT", "TOK_TOS", 
  "TOK_WILLINGNESS", "TOK_IPCCON", "TOK_USEHYST", "TOK_HYSTSCALE", 
  "TOK_HYSTUPPER", "TOK_HYSTLOWER", "TOK_POLLRATE", "TOK_TCREDUNDANCY", 
  "TOK_MPRCOVERAGE", "TOK_PLNAME", "TOK_PLPARAM", "TOK_IP4BROADCAST", 
  "TOK_IP6ADDRTYPE", "TOK_IP6MULTISITE", "TOK_IP6MULTIGLOBAL", 
  "TOK_HELLOINT", "TOK_HELLOVAL", "TOK_TCINT", "TOK_TCVAL", "TOK_MIDINT", 
  "TOK_MIDVAL", "TOK_HNAINT", "TOK_HNAVAL", "TOK_IP4_ADDR", 
  "TOK_IP6_ADDR", "TOK_COMMENT", "$accept", "conf", "stmt", "block", 
  "hna4body", "hna4stmts", "hna6body", "hna6stmts", "ifbody", "ifstmts", 
  "ifstmt", "isetbody", "isetstmts", "isetstmt", "plbody", "plstmts", 
  "plstmt", "isetblock", "isetip4br", "isetip6addrt", "isetip6mults", 
  "isetip6multg", "isethelloint", "isethelloval", "isettcint", 
  "isettcval", "isetmidint", "isetmidval", "isethnaint", "isethnaval", 
  "idebug", "iipversion", "ihna4entry", "ihna6entry", "ifentry", "bnoint", 
  "atos", "awillingness", "bipccon", "busehyst", "fhystscale", 
  "fhystupper", "fhystlower", "fpollrate", "atcredundancy", 
  "amprcoverage", "plname", "plparam", "vcomment", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    46,    47,    47,    47,    48,    48,    48,    48,    48,
      48,    48,    48,    48,    48,    48,    48,    48,    48,    49,
      49,    49,    49,    49,    50,    51,    51,    52,    53,    53,
      54,    55,    55,    56,    56,    57,    58,    58,    59,    59,
      59,    59,    59,    59,    59,    59,    59,    59,    59,    59,
      59,    60,    61,    61,    62,    62,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     3,     0,     2,     3,     0,     2,
       3,     0,     2,     1,     1,     3,     0,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     0,     2,     1,     1,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       2,     0,     1,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    88,     4,     3,     0,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    70,
      71,    25,    19,    28,    20,    52,    22,    31,    21,    57,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    36,    23,     0,     0,     0,     0,     0,    24,     0,
      26,    27,     0,    29,    51,     0,     0,    53,    54,    55,
      56,    30,     0,    32,    33,    34,    35,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    37,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    38,    72,    73,    86,     0,    74,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      87
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,     1,    22,    23,    42,    63,    44,    64,    48,    66,
      83,    62,    67,    99,    46,    65,    77,    24,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
      25,    26,    70,    73,    84,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    78,    79,    38
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -59
static const yysigned_char yypact[] =
{
     -59,     0,   -59,     3,    40,    45,    46,    47,    48,    49,
      -5,    50,    51,    43,    44,    52,    53,    54,    55,    57,
      58,   -59,   -59,   -59,    56,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,     1,     2,    37,    -3,    -2,   -59,    11,
     -59,   -59,    61,   -59,   -59,    63,    64,   -59,   -59,   -59,
     -59,   -59,    65,   -59,   -59,   -59,   -59,    13,    62,    29,
      30,    67,    68,    69,    70,    71,    72,    73,    75,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,    78,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,
     -59,   -59,   -59,   -59,   -59,   -59,   -59,   -59,   -58
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
       2,    81,    86,    82,    50,    68,    71,    80,    85,   112,
      39,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    74,    21,    21,    69,    21,    72,    40,    41,    43,
      45,    47,    53,    54,   113,    49,   118,    51,    52,    61,
      55,    56,    57,    58,    59,    60,    75,    76,   114,   115,
     116,   117,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,    21,   129,   130
};

static const unsigned char yycheck[] =
{
       0,     4,     4,     6,     9,     4,     4,    65,    66,    67,
       7,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,     4,    45,    45,    43,    45,    44,     7,     3,     3,
       3,     3,     9,     9,    43,     6,    43,     7,     7,     3,
       8,     8,     8,     8,     7,     7,    29,    30,     7,     6,
       6,     6,    10,    44,    44,     8,     8,     8,     8,     8,
       8,     8,    45,     8,     6
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    47,     0,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    45,    48,    49,    63,    76,    77,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    94,     7,
       7,     3,    50,     3,    52,     3,    60,     3,    54,     6,
       9,     7,     7,     9,     9,     8,     8,     8,     8,     7,
       7,     3,    57,    51,    53,    61,    55,    58,     4,    43,
      78,     4,    44,    79,     4,    29,    30,    62,    92,    93,
      94,     4,     6,    56,    80,    94,     4,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    59,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    94,    43,     7,     6,     6,     6,    43,    10,
      44,    44,     8,     8,     8,     8,     8,     8,     8,     8,
       6
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)         \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (cinluded).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylineno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylineno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 57:
#line 186 "oparse.y"
    {
  struct if_config_options *io = get_default_if_config();
  if(io == NULL)
    {
      fprintf(stderr, "Out of memory(ADD IFRULE)\n");
      YYABORT;
    }

  if(PARSER_DEBUG) printf("Interface setup: \"%s\"\n", yyvsp[0]->string);
  
  io->name = yyvsp[0]->string;
  
  
  /* Queue */
  io->next = cnf->if_options;
  cnf->if_options = io;

  free(yyvsp[0]);
;}
    break;

  case 58:
#line 209 "oparse.y"
    {
  struct in_addr in;

  if(PARSER_DEBUG) printf("\tIPv4 broadcast: %s\n", yyvsp[0]->string);

  if(inet_aton(yyvsp[0]->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[-1]->string);
      exit(EXIT_FAILURE);
    }

  cnf->if_options->ipv4_broadcast.v4 = in.s_addr;

  free(yyvsp[0]->string);
  free(yyvsp[0]);
;}
    break;

  case 59:
#line 228 "oparse.y"
    {
  cnf->if_options->ipv6_addrtype = yyvsp[0]->boolean;
  
  free(yyvsp[0]);
;}
    break;

  case 60:
#line 236 "oparse.y"
    {
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("\tIPv6 site-local multicast: %s\n", yyvsp[0]->string);

  if(inet_pton(AF_INET6, yyvsp[0]->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[0]->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&cnf->if_options->ipv6_multi_site.v6, &in6, sizeof(struct in6_addr));


  free(yyvsp[0]->string);
  free(yyvsp[0]);
;}
    break;

  case 61:
#line 256 "oparse.y"
    {
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("\tIPv6 global multicast: %s\n", yyvsp[0]->string);

  if(inet_pton(AF_INET6, yyvsp[0]->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[0]->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&cnf->if_options->ipv6_multi_glbl.v6, &in6, sizeof(struct in6_addr));


  free(yyvsp[0]->string);
  free(yyvsp[0]);
;}
    break;

  case 62:
#line 274 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tHELLO interval: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->hello_params.emission_interval = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 63:
#line 281 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tHELLO validity: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->hello_params.validity_time = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 64:
#line 288 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tTC interval: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->tc_params.emission_interval = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 65:
#line 295 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tTC validity: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->tc_params.validity_time = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 66:
#line 302 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tMID interval: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->mid_params.emission_interval = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 67:
#line 309 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tMID validity: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->mid_params.validity_time = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 68:
#line 316 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tHNA interval: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->hna_params.emission_interval = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 69:
#line 323 "oparse.y"
    {
    if(PARSER_DEBUG) printf("\tHNA validity: %0.2f\n", yyvsp[0]->floating);
    cnf->if_options->hna_params.validity_time = yyvsp[0]->floating;
    free(yyvsp[0]);
;}
    break;

  case 70:
#line 332 "oparse.y"
    {

  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Debug levl AUTO\n");
    }
  else
    {
      cnf->debug_level = yyvsp[0]->integer;
      if(PARSER_DEBUG) printf("Debug level: %d\n", cnf->debug_level);
    }

  free(yyvsp[0]);
;}
    break;

  case 71:
#line 350 "oparse.y"
    {
  if((yyvsp[0]->integer != 4) && (yyvsp[0]->integer != 6))
    {
      fprintf(stderr, "IPversion must be 4 or 6!\n");
      YYABORT;
    }
  cnf->ip_version = yyvsp[0]->integer;
  if(PARSER_DEBUG) printf("IpVersion: %d\n", cnf->ip_version);
  free(yyvsp[0]);
;}
    break;

  case 72:
#line 364 "oparse.y"
    {
  struct hna4_entry *h = malloc(sizeof(struct hna4_entry));
  struct in_addr in;

  if(PARSER_DEBUG) printf("HNA IPv4 entry: %s/%s\n", yyvsp[-1]->string, yyvsp[0]->string);

  if(h == NULL)
    {
      fprintf(stderr, "Out of memory(HNA4)\n");
      YYABORT;
    }

  if(inet_aton(yyvsp[-1]->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[-1]->string);
      exit(EXIT_FAILURE);
    }
  h->net = in.s_addr;
  if(inet_aton(yyvsp[0]->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[-1]->string);
      exit(EXIT_FAILURE);
    }
  h->netmask = in.s_addr;
  /* Queue */
  h->next = cnf->hna4_entries;
  cnf->hna4_entries = h;

  free(yyvsp[-1]->string);
  free(yyvsp[-1]);
  free(yyvsp[0]->string);
  free(yyvsp[0]);

;}
    break;

  case 73:
#line 400 "oparse.y"
    {
  struct hna6_entry *h = malloc(sizeof(struct hna6_entry));
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("HNA IPv6 entry: %s/%d\n", yyvsp[-1]->string, yyvsp[0]->integer);

  if(h == NULL)
    {
      fprintf(stderr, "Out of memory(HNA6)\n");
      YYABORT;
    }

  if(inet_pton(AF_INET6, yyvsp[-1]->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", yyvsp[-1]->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&h->net, &in6, sizeof(struct in6_addr));

  if((yyvsp[0]->integer < 0) || (yyvsp[0]->integer > 128))
    {
      fprintf(stderr, "Illegal IPv6 prefix length %d\n", yyvsp[0]->integer);
      exit(EXIT_FAILURE);
    }

  h->prefix_len = yyvsp[0]->integer;
  /* Queue */
  h->next = cnf->hna6_entries;
  cnf->hna6_entries = h;

  free(yyvsp[-1]->string);
  free(yyvsp[-1]);
  free(yyvsp[0]);

;}
    break;

  case 74:
#line 437 "oparse.y"
    {
  struct olsr_if *in = malloc(sizeof(struct olsr_if));
  
  if(in == NULL)
    {
      fprintf(stderr, "Out of memory(ADD IF)\n");
      YYABORT;
    }

  in->name = yyvsp[-1]->string;
  in->config = yyvsp[0]->string;

  if(PARSER_DEBUG) printf("Interface: %s Ruleset: %s\n", yyvsp[-1]->string, yyvsp[0]->string);

  /* Queue */
  in->next = cnf->interfaces;
  cnf->interfaces = in;

  free(yyvsp[-1]);
  free(yyvsp[0]);
;}
    break;

  case 75:
#line 461 "oparse.y"
    {
  if(PARSER_DEBUG) printf("Noint set to %d\n", yyvsp[0]->boolean);
  free(yyvsp[0]);
;}
    break;

  case 76:
#line 468 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Tos AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("TOS: %d\n", yyvsp[0]->integer);
    }
  free(yyvsp[0]);

;}
    break;

  case 77:
#line 483 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Willingness AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("Willingness: %d\n", yyvsp[0]->integer);
    }
  free(yyvsp[0]);

;}
    break;

  case 78:
#line 498 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("IPC allowed\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("IPC blocked\n");
    }
  free(yyvsp[0]);

;}
    break;

  case 79:
#line 514 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Hysteresis enabled\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("Hysteresis disabled\n");
    }
  free(yyvsp[0]);

;}
    break;

  case 80:
#line 530 "oparse.y"
    {
  cnf->hysteresis_param.scaling = yyvsp[0]->floating;
  if(PARSER_DEBUG) printf("Hysteresis Scaling: %0.2f\n", yyvsp[0]->floating);
  free(yyvsp[0]);
;}
    break;

  case 81:
#line 539 "oparse.y"
    {
  cnf->hysteresis_param.thr_high = yyvsp[0]->floating;
  if(PARSER_DEBUG) printf("Hysteresis UpperThr: %0.2f\n", yyvsp[0]->floating);
  free(yyvsp[0]);
;}
    break;

  case 82:
#line 548 "oparse.y"
    {
  cnf->hysteresis_param.thr_low = yyvsp[0]->floating;
  if(PARSER_DEBUG) printf("Hysteresis LowerThr: %0.2f\n", yyvsp[0]->floating);
  free(yyvsp[0]);
;}
    break;

  case 83:
#line 556 "oparse.y"
    {
  if(PARSER_DEBUG) printf("Pollrate %0.2f\n", yyvsp[0]->floating);
  cnf->pollrate = yyvsp[0]->floating;

  free(yyvsp[0]);
;}
    break;

  case 84:
#line 566 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("TC redundancy AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("TC redundancy %d\n", yyvsp[0]->integer);
      cnf->tc_redundancy = yyvsp[0]->integer;
    }
  free(yyvsp[0]);

;}
    break;

  case 85:
#line 582 "oparse.y"
    {
  if(yyvsp[0]->boolean == 1)
    {
      if(PARSER_DEBUG) printf("MPR coverage AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("MPR coverage %d\n", yyvsp[0]->integer);
      cnf->mpr_coverage = yyvsp[0]->integer;
    }
  free(yyvsp[0]);
;}
    break;

  case 86:
#line 598 "oparse.y"
    {
  struct plugin_entry *pe = malloc(sizeof(struct plugin_entry));
  
  if(pe == NULL)
    {
      fprintf(stderr, "Out of memory(ADD PL)\n");
      YYABORT;
    }

  pe->name = yyvsp[0]->string;
  
  if(PARSER_DEBUG) printf("Plugin: %s\n", yyvsp[0]->string);

  /* Queue */
  pe->next = cnf->plugins;
  cnf->plugins = pe;

  free(yyvsp[0]);
;}
    break;

  case 87:
#line 620 "oparse.y"
    {

    if(PARSER_DEBUG) printf("Plugin param key:\"%s\" val: \"%s\"\n", yyvsp[-1]->string, yyvsp[0]->string);

    free(yyvsp[-1]->string);
    free(yyvsp[-1]);
    free(yyvsp[0]->string);
    free(yyvsp[0]);
;}
    break;

  case 88:
#line 632 "oparse.y"
    {
    //if(PARSER_DEBUG) printf("Comment\n");
;}
    break;


    }

/* Line 999 of yacc.c.  */
#line 1712 "oparse.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("syntax error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyss < yyssp)
	    {
	      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
	      yydestruct (yystos[*yyssp], yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
      yydestruct (yytoken, &yylval);
      yychar = YYEMPTY;

    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      yyvsp--;
      yystate = *--yyssp;

      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 639 "oparse.y"


void yyerror (char *string)
{
  fprintf(stderr, "Config line %d: %s\n", current_line, string);
}

