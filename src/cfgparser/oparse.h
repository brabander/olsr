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




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



