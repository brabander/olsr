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
     TOK_INTERFACE = 271,
     TOK_IFSETTING = 272,
     TOK_IFSETUP = 273,
     TOK_NOINT = 274,
     TOK_TOS = 275,
     TOK_WILLINGNESS = 276,
     TOK_IPCCON = 277,
     TOK_USEHYST = 278,
     TOK_HYSTSCALE = 279,
     TOK_HYSTUPPER = 280,
     TOK_HYSTLOWER = 281,
     TOK_POLLRATE = 282,
     TOK_TCREDUNDANCY = 283,
     TOK_MPRCOVERAGE = 284,
     TOK_PLNAME = 285,
     TOK_PLPARAM = 286,
     TOK_IP4BROADCAST = 287,
     TOK_IP6ADDRTYPE = 288,
     TOK_IP6MULTISITE = 289,
     TOK_IP6MULTIGLOBAL = 290,
     TOK_HELLOINT = 291,
     TOK_HELLOVAL = 292,
     TOK_TCINT = 293,
     TOK_TCVAL = 294,
     TOK_MIDINT = 295,
     TOK_MIDVAL = 296,
     TOK_HNAINT = 297,
     TOK_HNAVAL = 298,
     TOK_IP4_ADDR = 299,
     TOK_IP6_ADDR = 300,
     TOK_COMMENT = 301
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
#define TOK_INTERFACE 271
#define TOK_IFSETTING 272
#define TOK_IFSETUP 273
#define TOK_NOINT 274
#define TOK_TOS 275
#define TOK_WILLINGNESS 276
#define TOK_IPCCON 277
#define TOK_USEHYST 278
#define TOK_HYSTSCALE 279
#define TOK_HYSTUPPER 280
#define TOK_HYSTLOWER 281
#define TOK_POLLRATE 282
#define TOK_TCREDUNDANCY 283
#define TOK_MPRCOVERAGE 284
#define TOK_PLNAME 285
#define TOK_PLPARAM 286
#define TOK_IP4BROADCAST 287
#define TOK_IP6ADDRTYPE 288
#define TOK_IP6MULTISITE 289
#define TOK_IP6MULTIGLOBAL 290
#define TOK_HELLOINT 291
#define TOK_HELLOVAL 292
#define TOK_TCINT 293
#define TOK_TCVAL 294
#define TOK_MIDINT 295
#define TOK_MIDVAL 296
#define TOK_HNAINT 297
#define TOK_HNAVAL 298
#define TOK_IP4_ADDR 299
#define TOK_IP6_ADDR 300
#define TOK_COMMENT 301




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



