/*
This Software is distributed with the following X11 License,
sometimes also known as MIT license.
 
Copyright (c) 2010 Miguel A. Ballicora

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

/*-- Intended to be modified to make public functions needed for the TB generator -------------------------*/

#if 0
#define SHARED_forbuilding
#endif

/*---------------------------------------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gtb-probe.h"

#if defined(SHARED_forbuilding)
	#include "gtb-prob2.h"
#else
	#define mySHARED static
	typedef unsigned char			SQ_CONTENT;
	typedef unsigned int 			SQUARE; 
#endif

#include "sysport.h"
#include "str64.h"
#include "possatt.h"

/*---------------------------------------------------------------------------------------------------------*/
/*#include "posit_t.h"*/

#define MAX_LISTSIZE 17
typedef unsigned 		sq_t;
typedef unsigned char 	pc_t;
typedef uint32_t		mv_t;

struct posit {
	sq_t 			ws[MAX_LISTSIZE];
	sq_t 			bs[MAX_LISTSIZE];
	pc_t 			wp[MAX_LISTSIZE];
	pc_t 			bp[MAX_LISTSIZE];
	sq_t 			ep;
	unsigned int 	stm;
	unsigned int 	cas;
};
typedef struct 	posit posit_t;


/*---------------------------------------------------------------------------------------------------------*/
/*#include "bool_t.h"*/

#if !defined(bool_t)
typedef int						bool_t;
#endif

#if !defined(TRUE)
#define TRUE ((bool_t)1)
#endif

#if !defined(FALSE)
#define FALSE ((bool_t)0)
#endif

/*--------- private if not external building code is present ----------------------------------------------*/

#if !defined(SHARED_forbuilding)

#define MAX_EGKEYS 145
#define SLOTSIZE 1
#define NOINDEX ((index_t)(-1))

typedef unsigned short int 	dtm_t;
typedef int 				index_t;

enum Loading_status {	
				STATUS_ABSENT = 0, 
				STATUS_STATICRAM = 1, 
				STATUS_MALLOC = 2,
				STATUS_FILE   = 3, 
				STATUS_REJECT    = 4
};

struct endgamekey {
	int 		id;
	const char *str;
	index_t 	maxindex;
	index_t 	slice_n;
	void   		(*itopc) (index_t, SQUARE *, SQUARE *);
	bool_t 		(*pctoi) (const SQUARE *, const SQUARE *, index_t *);
	dtm_t *		egt_w;
	dtm_t *		egt_b;
	FILE *		fd;
	int 		status;
	int			pathn; 
};

/*
static struct 	endgamekey egkey[];
static int 		bettarr [2] [8] [8];
*/

#endif

/*----------------------------------------------------------------------------------------------------------*/

/* array for better moves */
mySHARED int		bettarr [2] [8] [8];


/*------------ ENUMS ----------------------------------------------------------*/

enum Mask_values {	
					RESMASK  = tb_RESMASK, 
					INFOMASK = tb_INFOMASK, 
					PLYSHIFT = tb_PLYSHIFT  
};

enum Info_values {	
					iDRAW    = tb_DRAW, 
					iWMATE   = tb_WMATE, 
					iBMATE   = tb_BMATE, 
					iFORBID  = tb_FORBID,
		 
					iDRAWt   = tb_DRAW  |4, 
					iWMATEt  = tb_WMATE |4, 
					iBMATEt  = tb_BMATE |4, 
					iUNKNOWN = tb_UNKNOWN,

					iUNKNBIT = (1<<2)
};

/*-----------------------------------------------------------------------------*/

/*---------- needed from maindef.h -----------*/

#define WHITES (1u<<6)
#define BLACKS (1u<<7)

#define NOPIECE 0u
#define PAWN    1u
#define KNIGHT  2u
#define BISHOP  3u
#define ROOK    4u
#define QUEEN   5u
#define KING    6u

#define WH 0
#define BL 1
#define Opp(x) ((x)^1)
#define wK (KING   | WHITES)

/*-------------------
       SQUARES
  -------------------*/

/* from 1-63 different squares posibles   */

/*squares*/
enum SQUARES {
	A1,B1,C1,D1,E1,F1,G1,H1,
	A2,B2,C2,D2,E2,F2,G2,H2,
	A3,B3,C3,D3,E3,F3,G3,H3,
	A4,B4,C4,D4,E4,F4,G4,H4,
	A5,B5,C5,D5,E5,F5,G5,H5,
	A6,B6,C6,D6,E6,F6,G6,H6,
	A7,B7,C7,D7,E7,F7,G7,H7,
	A8,B8,C8,D8,E8,F8,G8,H8,
	NOSQUARE,
	ERRSQUARE = 128
};

/*-------- end needed from maindef.h -----------*/
 
#if !defined(NDEBUG)
#define NDEBUG
#endif
#ifdef DEBUG
#undef NDEBUG
#endif
#include "assert.h"

#undef BUILD_CODE
#if 0
#define BUILD_CODE
#endif

static bool_t TB_compression = FALSE;
static bool_t TB_cache_on = TRUE;

/*************************************************\
|
|				COMPRESSION SCHEMES 
|
\*************************************************/

#include "decode.h"

static const char *const Extension[] = {
							 ".gtb.cp0"
							,".gtb.cp1"
							,".gtb.cp2"
							,".gtb.cp3"
							,".gtb.cp4"
							,".gtb.cp5"
							,".gtb.cp6"
							,".gtb.cp7"
							,".gtb.cp8"
							,".gtb.cp9"
};

/*************************************************\
|
|					MOVES 
|
\*************************************************/

enum move_kind {
		NORMAL_MOVE = 0,
		CASTLE_MOVE,
		PASSNT_MOVE,
		PROMOT_MOVE
};

enum move_content {
		NOMOVE = 0	
};

#define MV_TYPE(mv)   ( (BYTE)       ((mv) >>6 & 3 )      )
#define MV_TO(mv)     ( (SQUARE)     ((mv) >>8 & 63)      )
#define MV_PT(mv)     ( (SQ_CONTENT) ((mv) >>(3+16) &7  ) )
#define MV_TK(mv)     ( (SQ_CONTENT) ((mv) >>(6+16) &7  ) )
#define MV_FROM(mv)   ( (SQUARE)     ((mv)     & 63)      )

/*
|   move,type,color,piece,from,to,taken,promoted
*------------------------------------------------------------------*/

#define MV_BUILD(mv,ty,co,pc,fr,to,tk,pm) (                        \
    (mv)    =  (fr)     | (to)<< 8      | (ty)<<  6     | (co)<<8  \
            |  (pc)<<16 | (pm)<< (3+16) | (tk)<< (6+16)            \
)

#define MV_ADD_TOTK(mv,to,tk) (          \
     mv     |= (uint32_t)(to) << 8       \
            |  (uint32_t)(tk) << (6+16)  \
)

#define map88(x)    (   (x) + ((x)&070)        )
#define unmap88(x)  ( ( (x) + ((x)& 07) ) >> 1 )

/*************************************************\
|
|				STATIC VARIABLES 
|
\*************************************************/

static int GTB_scheme = 4;

/*************************************************\
|
|	needed for 
|	PRE LOAD CACHE AND DEPENDENT FUNCTIONS 
|
\*************************************************/

#define EGTB_MAXBLOCKSIZE 65536
#define GTB_MAXOPEN 4

static bool_t 			Uncompressed = TRUE;
static unsigned char 	Buffer_zipped [EGTB_MAXBLOCKSIZE];
static unsigned char 	Buffer_packed [EGTB_MAXBLOCKSIZE];

/*static bool_t 			EGTB_PROBING_HARD = TRUE;*/
static int				zipinfo_init (void);
static void 			zipinfo_done (void);



enum Flip_flags {
		WE_FLAG = 1, NS_FLAG = 2,  NW_SE_FLAG = 4
}; /* used in flipt */

struct filesopen {
	int n;
	int key[GTB_MAXOPEN];
};

/* STATIC GLOBALS */

static struct filesopen 	fd;

static bool_t 	TB_INITIALIZED = FALSE;
static bool_t	TBCACHE_INITIALIZED = FALSE;

/* LOCKS */
static mythread_mutex_t	Egtb_lock;

#ifdef BUILD_CODE
/*--------------------------------*\
|
|			T I M E R
|
*---------------------------------*/
static tickcounter_t Timer_start[2];
static void 	timer_reset(int x) {
    Timer_start[x] = myclock();
}
static double 	timer_get  (int x) {
    double t = (double)myclock();
    return (t - Timer_start[x] )/(double)(ticks_per_sec());
}
#endif

/****************************************************************************\
 *
 *
 *			DEBUGGING or PRINTING ZONE
 *
 *
 ****************************************************************************/

#if 0
#define FOLLOW_EGTB
#ifndef DEBUG
#define DEBUG
#endif
#endif

#define validsq(x) ((x) >= A1 && (x) <= H8)

#if defined(DEBUG)
static void 	print_pos (const sq_t *ws, const sq_t *bs, const pc_t *wp, const pc_t *bp);
#endif

#if defined(BUILD_CODE) || defined(DEBUG) || defined(FOLLOW_EGTB)
static void 	output_state (unsigned int stm, const SQUARE *wSQ, const SQUARE *bSQ, 
								const SQ_CONTENT *wPC, const SQ_CONTENT *bPC);
static const char *Square_str[64] = {
 	"a1","b1","c1","d1","e1","f1","g1","h1",
 	"a2","b2","c2","d2","e2","f2","g2","h2",
 	"a3","b3","c3","d3","e3","f3","g3","h3",
 	"a4","b4","c4","d4","e4","f4","g4","h4",
 	"a5","b5","c5","d5","e5","f5","g5","h5",
 	"a6","b6","c6","d6","e6","f6","g6","h6",
 	"a7","b7","c7","d7","e7","f7","g7","h7",
 	"a8","b8","c8","d8","e8","f8","g8","h8"
};
static const char *P_str[] = {
	"--", "P", "N", "B", "R", "Q", "K"
};
#endif

#ifdef FOLLOW_EGTB
	#define STAB
	#define STABCONDITION 1 /*(stm == BL && whiteSQ[0]==H1 && whiteSQ[1]==D1 && whiteSQ[2]==D3 && blackSQ[0]==C2 )*/
	static bool_t GLOB_REPORT = TRUE;
#endif

#if defined(BUILD_CODE) || defined(FOLLOW_EGTB)
static const char *Info_str[8] = {	
	" Draw", " Wmate", " Bmate", "Illegal", 
	"~Draw", "~Wmate", "~Bmate", "Unknown" 
};
#endif

static void		list_index (void);
static void 	fatal_error(void) {
    exit(EXIT_FAILURE);
}

#ifdef STAB
	#define FOLLOW_LU(x,y)  {if (GLOB_REPORT) printf ("************** %s: %lu\n", (x), (long unsigned)(y));}
#else
	#define FOLLOW_LU(x,y)
#endif

#ifdef STAB
	#define FOLLOW_LULU(x,y,z)  {if (GLOB_REPORT) printf ("************** %s: %lu, %lu\n", (x), (long unsigned)(y), (long unsigned)(z));}
#else
	#define FOLLOW_LULU(x,y,z)
#endif

#ifdef STAB
	#define FOLLOW_label(x) {if (GLOB_REPORT) printf ("************** %s\n", (x));}
#else
	#define FOLLOW_label(x)
#endif

#ifdef STAB
	#define FOLLOW_DTM(msg,dtm)  {if (GLOB_REPORT) printf ("************** %s: %lu, info:%s, plies:%lu \n"\
	, (msg), (long unsigned)(dtm), (Info_str[(dtm)&INFOMASK]), (long unsigned)((dtm)>>PLYSHIFT)\
	);}
#else
	#define FOLLOW_DTM(msg,dtm)
#endif


/*--------------------------------*\
|
|
|		INDEXING FUNCTIONS
|
|
*---------------------------------*/

static void tbcache_reset_counters (void);

#define NO_KKINDEX NOINDEX
#define MAX_KKINDEX 462
#define MAX_PPINDEX 576
#define MAX_PpINDEX (24 * 48)
/*1128*/
#define MAX_AAINDEX ((63-62) + (62 * (127-62)/2) - 1 + 1)
#define MAX_AAAINDEX (64*21*31)
#define MAX_PP48_INDEX (1128)
/* (24*23*22/6) + 24 * (24*23/2) */
#define MAX_PPP48_INDEX 8648

/* VARIABLES */

static int 				kkidx [64] [64];
static int 				wksq [MAX_KKINDEX];
static int 				bksq [MAX_KKINDEX];
static int 				ppidx [24] [48];
static unsigned int 	pp_hi24 [MAX_PPINDEX];
static unsigned int 	pp_lo48 [MAX_PPINDEX];
static unsigned int 	flipt [64] [64];
static int 				aaidx [64] [64];
static unsigned char 	aabase [MAX_AAINDEX];
static index_t 			pp48_idx[48][48];
static sq_t				pp48_sq_x[MAX_PP48_INDEX];
static sq_t				pp48_sq_y[MAX_PP48_INDEX]; 
static index_t 			ppp48_idx[48][48][48];
static uint8_t			ppp48_sq_x[MAX_PPP48_INDEX];
static uint8_t			ppp48_sq_y[MAX_PPP48_INDEX]; 
static uint8_t			ppp48_sq_z[MAX_PPP48_INDEX]; 

#ifdef BUILD_CODE
static	uint64_t		Moves_avail_full = 0;
static	uint64_t		Moves_avail_easy = 0;	
#endif

/* FUNCTIONS */

static void 	init_indexing (int verbosity);
static void 	norm_kkindex (SQUARE x, SQUARE y, /*@out@*/ SQUARE *pi, /*@out@*/ SQUARE *pj);
static void 	pp_putanchorfirst (SQUARE a, SQUARE b, /*@out@*/ SQUARE *out_anchor, /*@out@*/ SQUARE *out_loosen);

static SQUARE 	wsq_to_pidx24 (SQUARE pawn);
static SQUARE 	wsq_to_pidx48 (SQUARE pawn);
static SQUARE 	pidx24_to_wsq (unsigned int a);
static SQUARE 	pidx48_to_wsq (unsigned int a);

static SQUARE 	flipWE    (SQUARE x) {
    return x ^  07;
}
static SQUARE 	flipNS    (SQUARE x) {
    return x ^ 070;
}
static SQUARE 	flipNW_SE (SQUARE x) {
    return ((x&7)<<3) | (x>>3);
}
static SQUARE 	getcol    (SQUARE x) {
    return x &  7;
}
static SQUARE 	getrow    (SQUARE x) {
    return x >> 3;
}
static bool_t 	in_queenside (sq_t x) {
    return 0 == (x & (1<<2));
}

/* 1:0 */
static void 	kxk_indextopc   (index_t i, SQUARE *pw, SQUARE *pb);

/* 2:0 */
static void 	kabk_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);
static void 	kakb_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaak_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);

/* 2:1 */
static void 	kabkc_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaakb_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/* 3:0 */
static void 	kabck_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaabk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaaak_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kabbk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/* one pawn */
static void 	kpk_indextopc   (index_t i, SQUARE *pw, SQUARE *pb);

/* 1:1 one pawn */
static void 	kakp_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);

/* 2:0 one pawn */
static void 	kapk_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);

/* 2:0 two pawns */
static void 	kppk_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);

/*  2:1 one pawn */
static void		kapkb_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kabkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaakp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/*  2:1 + 3:0 two pawns */
static void		kppka_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kappk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kapkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/*  3:0 one pawn */
static void		kabpk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kaapk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/*  three pawns */
static void		kpppk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);
static void		kppkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

/* 1:1 two pawns */
static void 	kpkp_indextopc  (index_t i, SQUARE *pw, SQUARE *pb);

/* corresponding pc to index */
static bool_t 	kxk_pctoindex   (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kabk_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kakb_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kpk_pctoindex   (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kakp_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kapk_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t   kppk_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kaak_pctoindex  (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kabkc_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);

static bool_t 	kaakb_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);/**/

static bool_t 	kabck_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kaabk_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);/**/
static bool_t 	kaaak_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kabbk_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);/**/
static bool_t 	kapkb_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kabkp_pctoindex (const SQUARE *pw, const SQUARE *pb, /*@out@*/ index_t *out);
static bool_t 	kaakp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kppka_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out);
static bool_t 	kappk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kapkp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kabpk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kaapk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kppkp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kpppk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out);
static bool_t 	kpkp_pctoindex  (const SQUARE *pw,     const SQUARE *pb, /*@out@*/ index_t *out);

/* testing functions */
static bool_t 	test_kppk  (void);
static bool_t 	test_kaakb (void);
static bool_t 	test_kaabk (void);
static bool_t 	test_kaaak (void);
static bool_t 	test_kabbk (void);
static bool_t 	test_kapkb (void);
static bool_t 	test_kabkp (void);
static bool_t 	test_kppka (void);
static bool_t 	test_kappk (void);
static bool_t 	test_kapkp (void);
static bool_t 	test_kabpk (void);
static bool_t 	test_kaapk (void);
static bool_t 	test_kaakp (void);
static bool_t 	test_kppkp (void);
static bool_t 	test_kpppk (void);

static unsigned flip_type (SQUARE x, SQUARE y);
static index_t 	init_kkidx (void);
static index_t	init_ppidx (void);
static void    	init_flipt (void);
static index_t 	init_aaidx (void);
static index_t	init_aaa   (void);
static index_t	init_pp48_idx (void);
static index_t	init_ppp48_idx (void);

#ifdef BUILD_CODE
static void   (*indextopc) (index_t, SQUARE *, SQUARE *);
static bool_t (*pctoindex) (const SQUARE *, const SQUARE *, index_t *);
static index_t Maxindex;
#endif

enum TB_INDEXES
	 {	 MAX_KXK 	= MAX_KKINDEX*64 
		,MAX_kabk 	= MAX_KKINDEX*64*64 
		,MAX_kakb 	= MAX_KKINDEX*64*64
		,MAX_kpk	= 24*64*64
		,MAX_kakp	= 24*64*64*64
		,MAX_kapk	= 24*64*64*64		
		,MAX_kppk	= MAX_PPINDEX*64*64
		,MAX_kpkp	= MAX_PpINDEX*64*64
		,MAX_kaak	= MAX_KKINDEX*MAX_AAINDEX
		,MAX_kabkc	= MAX_KKINDEX*64*64*64
		,MAX_kabck	= MAX_KKINDEX*64*64*64
		,MAX_kaakb	= MAX_KKINDEX*MAX_AAINDEX*64
		,MAX_kaabk	= MAX_KKINDEX*MAX_AAINDEX*64
		,MAX_kabbk  = MAX_KKINDEX*MAX_AAINDEX*64
		,MAX_kaaak	= MAX_KKINDEX*MAX_AAAINDEX
		,MAX_kapkb  = 24*64*64*64*64
		,MAX_kabkp  = 24*64*64*64*64
		,MAX_kabpk  = 24*64*64*64*64
		,MAX_kppka  = MAX_kppk*64
		,MAX_kappk  = MAX_kppk*64
		,MAX_kapkp  = MAX_kpkp*64
		,MAX_kaapk  = 24*MAX_AAINDEX*64*64
		,MAX_kaakp  = 24*MAX_AAINDEX*64*64
		,MAX_kppkp  = 24*MAX_PP48_INDEX*64*64
		,MAX_kpppk  = MAX_PPP48_INDEX*64*64	
};

#if defined(SHARED_forbuilding)
extern index_t 
biggest_memory_needed (void) {
    return MAX_kabkc;
}
#endif

/*--------------------------------*\
|
|
|		CACHE PROTOTYPES
|
|
*---------------------------------*/
#if !defined(SHARED_forbuilding)
mySHARED bool_t		get_dtm (int key, int side, index_t idx, dtm_t *out, bool_t probe_hard);
#endif
static bool_t	 	get_dtm_from_cache (int key, int side, index_t idx, dtm_t *out);
/*static double		tbcache_efficiency (void);*/

/*--------------------------------*\
|
|
|			INIT
|
|
*---------------------------------*/

static void 	fd_init (struct filesopen *pfd);
static void		fd_done (struct filesopen *pfd);

static void		RAM_egtbfree (void);

/*--------------------------------------------------------------------------*/
#if !defined(SHARED_forbuilding)
mySHARED void   		egtb_freemem (int i);
#endif

#ifdef BUILD_CODE
static bool_t 		egtb_reservemem (int key, long int n, size_t sz);
static dtm_t *		egtab [2];
static SQ_CONTENT 	whitePC[MAX_LISTSIZE], blackPC[MAX_LISTSIZE];
#endif

mySHARED struct endgamekey egkey[] = {

{0, "kqk",  MAX_KXK,  1, kxk_indextopc,  kxk_pctoindex,  NULL ,  NULL   ,NULL ,0, 0 },
{1, "krk",  MAX_KXK,  1, kxk_indextopc,  kxk_pctoindex,  NULL ,  NULL   ,NULL ,0, 0 },
{2, "kbk",  MAX_KXK,  1, kxk_indextopc,  kxk_pctoindex,  NULL ,  NULL   ,NULL ,0, 0 },
{3, "knk",  MAX_KXK,  1, kxk_indextopc,  kxk_pctoindex,  NULL ,  NULL   ,NULL ,0, 0 },
{4, "kpk",  MAX_kpk,  24,kpk_indextopc,  kpk_pctoindex,  NULL ,  NULL   ,NULL ,0, 0 },
	/* 4 pieces */	
{5, "kqkq", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{6, "kqkr", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{7, "kqkb", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{8, "kqkn", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	

{9, "krkr", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{10,"krkb", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{11,"krkn", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{12,"kbkb", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{13,"kbkn", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{14,"knkn", MAX_kakb, 1, kakb_indextopc, kakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
	/**/		
{15,"kqqk", MAX_kaak, 1, kaak_indextopc, kaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{16,"kqrk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{17,"kqbk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{18,"kqnk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	

{19,"krrk", MAX_kaak, 1, kaak_indextopc, kaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{20,"krbk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{21,"krnk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{22,"kbbk", MAX_kaak, 1, kaak_indextopc, kaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
{23,"kbnk", MAX_kabk, 1, kabk_indextopc, kabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{24,"knnk", MAX_kaak, 1, kaak_indextopc, kaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },	
	/**/	
	/**/
{25,"kqkp", MAX_kakp, 24,kakp_indextopc, kakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{26,"krkp", MAX_kakp, 24,kakp_indextopc, kakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{27,"kbkp", MAX_kakp, 24,kakp_indextopc, kakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{28,"knkp", MAX_kakp, 24,kakp_indextopc, kakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
	/**/
{29,"kqpk", MAX_kapk, 24,kapk_indextopc, kapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{30,"krpk", MAX_kapk, 24,kapk_indextopc, kapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{31,"kbpk", MAX_kapk, 24,kapk_indextopc, kapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{32,"knpk", MAX_kapk, 24,kapk_indextopc, kapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
	/**/	
{33,"kppk", MAX_kppk, MAX_PPINDEX ,kppk_indextopc, kppk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
	/**/
{34,"kpkp", MAX_kpkp, MAX_PpINDEX ,kpkp_indextopc, kpkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
	/**/
	/**/
	/* 5 pieces */
{ 35,"kqqqk", MAX_kaaak, 1, kaaak_indextopc, kaaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 36,"kqqrk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 37,"kqqbk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 38,"kqqnk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 39,"kqrrk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 40,"kqrbk", MAX_kabck, 1, kabck_indextopc, kabck_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 41,"kqrnk", MAX_kabck, 1, kabck_indextopc, kabck_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 42,"kqbbk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 43,"kqbnk", MAX_kabck, 1, kabck_indextopc, kabck_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 44,"kqnnk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 45,"krrrk", MAX_kaaak, 1, kaaak_indextopc, kaaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 46,"krrbk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 47,"krrnk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 48,"krbbk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 49,"krbnk", MAX_kabck, 1, kabck_indextopc, kabck_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 50,"krnnk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 51,"kbbbk", MAX_kaaak, 1, kaaak_indextopc, kaaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 52,"kbbnk", MAX_kaabk, 1, kaabk_indextopc, kaabk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 53,"kbnnk", MAX_kabbk, 1, kabbk_indextopc, kabbk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 54,"knnnk", MAX_kaaak, 1, kaaak_indextopc, kaaak_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 55,"kqqkq", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 56,"kqqkr", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 57,"kqqkb", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 58,"kqqkn", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 59,"kqrkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 60,"kqrkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 61,"kqrkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 62,"kqrkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 63,"kqbkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 64,"kqbkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 65,"kqbkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 66,"kqbkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 67,"kqnkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 68,"kqnkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 69,"kqnkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 70,"kqnkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 71,"krrkq", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 72,"krrkr", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 73,"krrkb", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 74,"krrkn", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 75,"krbkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 76,"krbkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 77,"krbkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 78,"krbkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 79,"krnkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 80,"krnkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 81,"krnkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 82,"krnkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 83,"kbbkq", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 84,"kbbkr", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 85,"kbbkb", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 86,"kbbkn", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 87,"kbnkq", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 88,"kbnkr", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 89,"kbnkb", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 90,"kbnkn", MAX_kabkc, 1, kabkc_indextopc, kabkc_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 91,"knnkq", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 92,"knnkr", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 93,"knnkb", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 94,"knnkn", MAX_kaakb, 1, kaakb_indextopc, kaakb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{ 95,"kqqpk", MAX_kaapk, 24, kaapk_indextopc, kaapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 96,"kqrpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 97,"kqbpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 98,"kqnpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{ 99,"krrpk", MAX_kaapk, 24, kaapk_indextopc, kaapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{100,"krbpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{101,"krnpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{102,"kbbpk", MAX_kaapk, 24, kaapk_indextopc, kaapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{103,"kbnpk", MAX_kabpk, 24, kabpk_indextopc, kabpk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{104,"knnpk", MAX_kaapk, 24, kaapk_indextopc, kaapk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{105,"kqppk", MAX_kappk, MAX_PPINDEX, kappk_indextopc, kappk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{106,"krppk", MAX_kappk, MAX_PPINDEX, kappk_indextopc, kappk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{107,"kbppk", MAX_kappk, MAX_PPINDEX, kappk_indextopc, kappk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{108,"knppk", MAX_kappk, MAX_PPINDEX, kappk_indextopc, kappk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{109,"kqpkq", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{110,"kqpkr", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{111,"kqpkb", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{112,"kqpkn", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{113,"krpkq", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{114,"krpkr", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{115,"krpkb", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{116,"krpkn", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{117,"kbpkq", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{118,"kbpkr", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{119,"kbpkb", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{120,"kbpkn", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{121,"knpkq", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{122,"knpkr", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{123,"knpkb", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{124,"knpkn", MAX_kapkb, 24, kapkb_indextopc, kapkb_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{125,"kppkq", MAX_kppka, MAX_PPINDEX, kppka_indextopc, kppka_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{126,"kppkr", MAX_kppka, MAX_PPINDEX, kppka_indextopc, kppka_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{127,"kppkb", MAX_kppka, MAX_PPINDEX, kppka_indextopc, kppka_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{128,"kppkn", MAX_kppka, MAX_PPINDEX, kppka_indextopc, kppka_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{129,"kqqkp", MAX_kaakp, 24, kaakp_indextopc, kaakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{130,"kqrkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{131,"kqbkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{132,"kqnkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{133,"krrkp", MAX_kaakp, 24, kaakp_indextopc, kaakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{134,"krbkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{135,"krnkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{136,"kbbkp", MAX_kaakp, 24, kaakp_indextopc, kaakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{137,"kbnkp", MAX_kabkp, 24, kabkp_indextopc, kabkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{138,"knnkp", MAX_kaakp, 24, kaakp_indextopc, kaakp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{139,"kqpkp", MAX_kapkp, MAX_PpINDEX, kapkp_indextopc, kapkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{140,"krpkp", MAX_kapkp, MAX_PpINDEX, kapkp_indextopc, kapkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{141,"kbpkp", MAX_kapkp, MAX_PpINDEX, kapkp_indextopc, kapkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{142,"knpkp", MAX_kapkp, MAX_PpINDEX, kapkp_indextopc, kapkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{143,"kppkp", MAX_kppkp, 24*MAX_PP48_INDEX, kppkp_indextopc, kppkp_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },
{144,"kpppk", MAX_kpppk, MAX_PPP48_INDEX, kpppk_indextopc, kpppk_pctoindex, NULL ,  NULL   ,NULL ,0, 0 },

{MAX_EGKEYS, NULL,  0,        1, NULL,           NULL,           NULL,   NULL   ,NULL ,0 ,0}

};

static int eg_was_open[MAX_EGKEYS];

static uint64_t Bytes_read = 0;

/****************************************************************************\
|
|
|						PATH MANAGEMENT ZONE
|
|
\****************************************************************************/

#define MAXPATHLEN tb_MAXPATHLEN
#define MAX_GTBPATHS 5

static char Gtbpath_0 [MAXPATHLEN+1] = "";
static char Gtbpath_1 [MAXPATHLEN+1] = "";
static char Gtbpath_2 [MAXPATHLEN+1] = "";
static char Gtbpath_3 [MAXPATHLEN+1] = "";
static char Gtbpath_4 [MAXPATHLEN+1] = "";

static int  Gtbpath_end_index = 0;

static char *	Gtbpath [MAX_GTBPATHS+1] = {Gtbpath_0, Gtbpath_1, Gtbpath_2, Gtbpath_3, Gtbpath_4, NULL};

static void
egtb_setpath (int x, const char *path)
{
	const char *s = path;
	char *t = Gtbpath_0;

	switch (x) {
		case 0: t = Gtbpath_0;	break;
		case 1: t = Gtbpath_1;	break;
		case 2: t = Gtbpath_2;	break;
		case 3: t = Gtbpath_3;	break;
		case 4: t = Gtbpath_4;	break;
		default:
			return;
		break;
	}
 
	while (*s != '\0' && (s-path) < MAXPATHLEN) {
		*t++ = *s++;
	}
	*t = '\0';
	return;	
}

static int
egtb_addpath (const char *p)
{
	if (Gtbpath_end_index < MAX_GTBPATHS) {
		egtb_setpath (Gtbpath_end_index++, p);	
		return 1;
	} else {
		return 0;
	}
}

/*---------------- EXTERNAL PATH MANAGEMENT --------------------------------*/

extern const char *tbpaths_getmain (void)
{
	return Gtbpath[0];
}

extern char **
tbpaths_init(void)
{
	char **newps;
	newps = malloc (sizeof (char *));
	if (newps != NULL) {
		newps[0] = NULL;
	}
	return newps;
}

extern char **
tbpaths_add(char **ps, char *newpath)
{
	int counter;
	char **newps;
	for (counter = 0; ps[counter] != NULL; counter++)
		; 

	newps =	realloc (ps, sizeof(char *) * (counter+2));
	if (newps != NULL) {
		newps [counter] = newpath;
		newps [counter+1] = NULL;
	}
	return newps;
}

extern char **
tbpaths_done(char **ps)
{
	if (ps != NULL) {
		free(ps);
	}
	return NULL;
}

/****************************************************************************\
 *
 *
 *						General Initialization Zone
 *
 *
 ****************************************************************************/

static void 	init_bettarr (void);

static void	eg_was_open_reset(void)
{
	int i;
	for (i = 0; i < MAX_EGKEYS; i++) {
		eg_was_open[i] = 0;
	}
}

static int	eg_was_open_count(void)
{
	int i, x;
	for (i = 0, x = 0; i < MAX_EGKEYS; i++) {
		x += eg_was_open[i];
	}
	return x;
}

static void path_system_reset(void) {Gtbpath_end_index = 0;}

static void
set_path_system (char **path)
{
	int i;
	bool_t ok = TRUE;
	path_system_reset();
	for (i = 0; ok && path[i] != NULL; i++) {
		ok = egtb_addpath (path[i]);
	}
}

extern void
tb_init (int verbosity, int decoding_scheme, char **paths)
{
	int zi;

	assert(!TB_INITIALIZED);

	set_path_system (paths);

	if (verbosity) { 
		int g;
		printf ("\nGTB PATHS\n");
		for (g = 0; Gtbpath[g] != NULL; g++) {
			char *p = Gtbpath[g];
			printf ("  #%d: %s\n", g, p);
		}
		fflush(stdout);
	}

	if (!reach_was_initialized())
		reach_init();

	attack_maps_init (); /* external initialization */

	init_indexing(0 /* no verbosity */);	
	init_bettarr();
	fd_init (&fd);
	
	GTB_scheme = decoding_scheme;
	Uncompressed = GTB_scheme == 0;
	TB_compression = !Uncompressed;

	if (GTB_scheme == 0) {
		Uncompressed = TRUE;
		TB_compression = FALSE;
	}

	set_decoding_scheme(GTB_scheme);

	if (verbosity) {
		printf ("\nGTB initialization\n");
		printf ("  Compression  Scheme = %d\n", GTB_scheme);
	}

	zi = zipinfo_init();

	if (verbosity) {
		if (!zi) {
			printf ("  Compression Indexes = **FAILED**\n");
		} else {
			switch (zi) {
				case 3: 
					printf ("  Compression Indexes (3-pc) = PASSED\n");
					printf ("  Compression Indexes (4-pc) = **FAILED**\n");
					printf ("  Compression Indexes (5-pc) = **FAILED**\n");
					break;
				case 4: 
					printf ("  Compression Indexes (3-pc) = PASSED\n");
					printf ("  Compression Indexes (4-pc) = PASSED\n");
					printf ("  Compression Indexes (5-pc) = **FAILED**\n");
					break;
				case 5: 
					printf ("  Compression Indexes (3-pc) = PASSED\n");
					printf ("  Compression Indexes (4-pc) = PASSED\n");
					printf ("  Compression Indexes (5-pc) = PASSED*\n");
					break;
				default:
				break;
			}
		}
	}

	eg_was_open_reset();
	Bytes_read = 0;

	mythread_mutex_init (&Egtb_lock);

	TB_INITIALIZED = TRUE;

	return;
}


extern bool_t
tb_is_initialized (void)
{
	return TB_INITIALIZED;
}


extern void
tb_done (void)
{
	assert(TB_INITIALIZED);
	fd_done (&fd);
	RAM_egtbfree();
	zipinfo_done();
	mythread_mutex_destroy (&Egtb_lock);
	TB_INITIALIZED = FALSE;
	return;
}


extern void
tb_restart(int verbosity, int decoding_scheme, char **paths)
{
	if (tb_is_initialized()) {
		tb_done();
	}
	tb_init(verbosity, decoding_scheme, paths);
	return;
}

/* whenever the program exits should release this memory */
static void
RAM_egtbfree (void)
{
	int i;
	for (i = 0; egkey[i].str != NULL; i++) {
		egtb_freemem (i);
	}
}

/*--------------------------------------------------------------------------*/

static void
init_bettarr (void)
{
/*
		iDRAW  = 0, iWMATE  = 1, iBMATE  = 2, iFORBID  = 3, 
		iDRAWt = 4, iWMATEt = 5, iBMATEt = 6, iUNKNOWN = 7
 */

	int temp[] = {
	/*White*/	
	/*iDRAW   vs*/
		1, 2, 1, 1,     2, 2, 2, 2,
	/*iWMATE  vs*/	
		1, 3, 1, 1,     1, 1, 1, 1,
	/*iBMATE  vs*/
		2, 2, 4, 1,     2, 2, 2, 2,
	/*iFORBID vs*/
		2, 2, 2, 2,     2, 2, 2, 2,
	
	/*iDRAWt  vs*/
		1, 2, 1, 1,     2, 2, 1, 2,
	/*iWMATEt vs*/	
		1, 2, 1, 1,     1, 3, 1, 1,
	/*iBMATEt vs*/
		1, 2, 1, 1,     2, 2, 4, 2,
	/*iUNKNOWN  */
		1, 2, 1, 1,     1, 2, 1, 2,

	/*Black*/
	/*iDRAW   vs*/
		1, 1, 2, 1,     2, 2, 2, 2,
	/*iWMATE  vs*/	
		2, 4, 2, 1,     2, 2, 2, 2,
	/*iBMATE  vs*/
		1, 1, 3, 1,     1, 1, 1, 1,
	/*iFORBID vs*/
		2, 2, 2, 2,     2, 2, 2, 2,
	
	/*iDRAWt  vs*/
		1, 1, 2, 1,     2, 1, 2, 2,
	/*iWMATEt vs*/	
		1, 1, 2, 1,     2, 4, 2, 2,
	/*iBMATEt vs*/
		1, 1, 2, 1,     1, 1, 3, 1,
	/*iUNKNOWN  */
		1, 1, 2, 1,     1, 1, 2, 2
	};

	int i, j, k, z;
	
	/* reset */
	z = 0;
	for (i = 0; i < 2; i++)
		for (j = 0; j < 8; j++) 
			for (k = 0; k < 8; k++)	
				bettarr [i][j][k] = temp[z++];

	return;
}

/*
|
|	Own File Descriptors
|
\*---------------------------------------------------------------------------*/

static void
fd_init (struct filesopen *pfd)
{
    int i;
	pfd->n = 0;
	for (i = 0; i < GTB_MAXOPEN; i++) {
		pfd->key[i] = -1;	
	}
}

static void
fd_done (struct filesopen *pfd)
{
    int i;
	int closingkey;
	FILE *finp;
	for (i = 0; i < pfd->n; i++) {
		closingkey = pfd->key[i];
		finp = egkey [closingkey].fd;
		fclose (finp);
		egkey[closingkey].fd = NULL;
		pfd->key[i] = -1;	
	}
	pfd->n = 0;
}

/****************************************************************************\
|
|
|								PROBE ZONE
|
|
\****************************************************************************/

#if !defined(SHARED_forbuilding)
/* shared with building routines */
mySHARED void 	list_sq_copy 	(const SQUARE *a, SQUARE *b);
mySHARED void 	list_pc_copy 	(const SQ_CONTENT *a, SQ_CONTENT *b);
mySHARED dtm_t 	inv_dtm 		(dtm_t x);
mySHARED bool_t 	egtb_get_id  	(SQ_CONTENT *w, SQ_CONTENT *b, long int *id);
mySHARED void 	list_sq_flipNS 	(SQUARE *s);
mySHARED dtm_t 	adjust_up (dtm_t dist);
mySHARED dtm_t 	bestx 			(unsigned int stm, dtm_t a, dtm_t b);
mySHARED void		sortlists (SQUARE *ws, SQ_CONTENT *wp);

mySHARED /*@NULL@*/ FILE * fd_openit (int key);

mySHARED dtm_t 			dtm_unpack (unsigned int stm, unsigned char packed);
mySHARED void  			unpackdist (dtm_t d, unsigned int *res, unsigned int *ply);
mySHARED dtm_t 			packdist (unsigned int inf, unsigned int ply);

mySHARED bool_t			fread_entry_packed 	(FILE *dest, unsigned int side, dtm_t *px);
mySHARED bool_t			fpark_entry_packed  (FILE *finp, int side, index_t max, index_t idx);
#endif

#ifdef BUILD_CODE
static unsigned char 	dtm_pack (unsigned int stm, dtm_t d);
static bool_t			fwrite_entry_packed (FILE *dest, unsigned int side, dtm_t x);
#endif



/* use only with probe */
static bool_t	egtb_get_dtm 	(int k, unsigned stm, const SQUARE *wS, const SQUARE *bS, bool_t probe_hard, dtm_t *dtm);
static void		removepiece (SQUARE *ys, SQ_CONTENT *yp, int j);
static bool_t 	egtb_filepeek (int key, int side, index_t idx, dtm_t *out_dtm);

static bool_t
tb_probe_	(unsigned int stm, 
			 SQUARE epsq,
			 const SQUARE *inp_wSQ, 
			 const SQUARE *inp_bSQ,
			 const SQ_CONTENT *inp_wPC, 
			 const SQ_CONTENT *inp_bPC,
			 bool_t probingtype,
			 /*@out@*/ unsigned *res, 
			 /*@out@*/ unsigned *ply);


extern bool_t
tb_probe_soft
			(unsigned int stm, 
			 SQUARE epsq,
			 unsigned castles,
			 const SQUARE *inp_wSQ, 
			 const SQUARE *inp_bSQ,
			 const SQ_CONTENT *inp_wPC, 
			 const SQ_CONTENT *inp_bPC,
			 /*@out@*/ unsigned *res, 
			 /*@out@*/ unsigned *ply)
{
	if (castles != 0) 
		return FALSE;
	return tb_probe_ (stm, epsq, inp_wSQ, inp_bSQ, inp_wPC, inp_bPC, FALSE, res, ply);
} 

extern bool_t
tb_probe_hard
			(unsigned int stm, 
			 SQUARE epsq,
			 unsigned castles,
			 const SQUARE *inp_wSQ, 
			 const SQUARE *inp_bSQ,
			 const SQ_CONTENT *inp_wPC, 
			 const SQ_CONTENT *inp_bPC,
			 /*@out@*/ unsigned *res, 
			 /*@out@*/ unsigned *ply)
{
	if (castles != 0) 
		return FALSE;
	return tb_probe_ (stm, epsq, inp_wSQ, inp_bSQ, inp_wPC, inp_bPC, TRUE, res, ply);
} 


static bool_t
tb_probe_	(unsigned int stm, 
			 SQUARE epsq,
			 const SQUARE *inp_wSQ, 
			 const SQUARE *inp_bSQ,
			 const SQ_CONTENT *inp_wPC, 
			 const SQ_CONTENT *inp_bPC,
			 bool_t probingtype,
			 /*@out@*/ unsigned *res, 
			 /*@out@*/ unsigned *ply)
{
	int i = 0, j = 0;
	long int id = -1;
	dtm_t dtm;

	SQUARE 		storage_ws [MAX_LISTSIZE], storage_bs [MAX_LISTSIZE];
	SQ_CONTENT  storage_wp [MAX_LISTSIZE], storage_bp [MAX_LISTSIZE];

	SQUARE     *ws = storage_ws;
	SQUARE     *bs = storage_bs;
	SQ_CONTENT *wp = storage_wp;
	SQ_CONTENT *bp = storage_bp;

	SQUARE     *xs;
	SQUARE     *ys;
	SQ_CONTENT *xp;
	SQ_CONTENT *yp;

	SQUARE 		tmp_ws [MAX_LISTSIZE], tmp_bs [MAX_LISTSIZE];
	SQ_CONTENT  tmp_wp [MAX_LISTSIZE], tmp_bp [MAX_LISTSIZE];

	SQUARE *temps;
	bool_t straight = FALSE;
	
	SQUARE capturer_a, capturer_b, xed = NOSQUARE;
	
	unsigned int plies;
	unsigned int inf;

	bool_t  okdtm  = TRUE;
	bool_t	okcall = TRUE;

	/************************************/

	assert (stm == WH || stm == BL);
	assert (inp_wPC[0] == KING && inp_bPC[0] == KING );
	assert ((epsq >> 3) == 2 || (epsq >> 3) == 5 || epsq == NOSQUARE);

	/*	THIS IS TEMPORARY, VALID ONLY FOR KK!! */
	if (inp_wPC[1] == NOPIECE && inp_bPC[1] == NOPIECE) {
		index_t dummy_i;
		bool_t b = kxk_pctoindex (inp_wSQ, inp_bSQ, &dummy_i);
		*res = b? iDRAW: iFORBID;
		*ply = 0;
		return TRUE;
	} 

	/* copy input */
	list_pc_copy (inp_wPC, wp);
	list_pc_copy (inp_bPC, bp);
	list_sq_copy (inp_wSQ, ws);
	list_sq_copy (inp_bSQ, bs);

	sortlists (ws, wp);
	sortlists (bs, bp);

	FOLLOW_label("EGTB_PROBE")

	if (egtb_get_id (wp, bp, &id)) {
		FOLLOW_LU("got ID",id)
		straight = TRUE;
	} else if (egtb_get_id (bp, wp, &id)) {
		FOLLOW_LU("rev ID",id)
		straight = FALSE;
		list_sq_flipNS (ws);
		list_sq_flipNS (bs);
        temps = ws;
        ws = bs;
        bs = temps;
		stm = Opp(stm);
	} else {
		#if defined(DEBUG)
		printf("did not get id...\n");
		output_state (stm, ws, bs, wp, bp);		
		#endif
		unpackdist (iFORBID, res, ply);
		return FALSE;
	}

	/* store position... */
	list_pc_copy (wp, tmp_wp);
	list_pc_copy (bp, tmp_bp);
	list_sq_copy (ws, tmp_ws);
	list_sq_copy (bs, tmp_bs);

	/* x will be stm and y will be stw */
	if (stm == WH) {
        xs = ws;
        xp = wp;
        ys = bs;
        yp = bp;
    } else {
        xs = bs;
        xp = bp;
        ys = ws;
        yp = wp;
	}

	okdtm = egtb_get_dtm (id, stm, ws, bs, probingtype, &dtm);

	FOLLOW_LU("dtmok?",okdtm)
	FOLLOW_DTM("dtm", dtm)

	if (okdtm) {

		capturer_a = NOSQUARE;
		capturer_b = NOSQUARE;

		if (epsq != NOSQUARE) {
			/* captured pawn, trick: from epsquare to captured */		
			xed = epsq ^ (1<<3);

			/* find index captured (j) */
			for (j = 0; ys[j] != NOSQUARE; j++) {
				if (ys[j] == xed) break;
			}	

			/* try first possible ep capture */
			if (0 == (0x88 & (map88(xed) + 1))) 
				capturer_a = xed + 1;
			/* try second possible ep capture */
			if (0 == (0x88 & (map88(xed) - 1))) 
				capturer_b = xed - 1;

			if (ys[j] == xed) {
	
				/* find capturers (i) */
				for (i = 0; xs[i] != NOSQUARE && okcall; i++) {

					if (xp[i]==PAWN && (xs[i]==capturer_a || xs[i]==capturer_b)) {
						dtm_t epscore = iFORBID;

						/* execute capture */
						xs[i] = epsq;
						removepiece (ys, yp, j);
					
						okcall = tb_probe_ (Opp(stm), NOSQUARE, ws, bs, wp, bp, probingtype, &inf, &plies);
						
						if (okcall) {
							epscore = packdist (inf, plies); 					
							epscore = adjust_up (epscore);

							/* chooses to ep or not */
							dtm = bestx (stm, epscore, dtm);
						}
	
						/* restore position */
						list_pc_copy (tmp_wp, wp);
						list_pc_copy (tmp_bp, bp);
						list_sq_copy (tmp_ws, ws);
						list_sq_copy (tmp_bs, bs);					
					}
				}
			} 
		} /* ep */

		if (straight) {
			unpackdist (dtm, res, ply);
		} else {
			unpackdist (inv_dtm (dtm), res, ply);
		}	 
	} 

	if (!okdtm || !okcall) {
		unpackdist (iFORBID, res, ply);
	}
	
	return okdtm && okcall;
} 



static bool_t
egtb_filepeek (int key, int side, index_t idx, dtm_t *out_dtm)
{
	FILE *finp;

#define USE_FD

	#if !defined(USE_FD)
	char buf[1024];
	char *filename = buf;
	#endif

	bool_t ok;
    dtm_t x=0;
	index_t maxindex  = egkey[key].maxindex;


	assert (Uncompressed);
	assert (side == WH || side == BL);
	assert (out_dtm != NULL);
	assert (idx >= 0);
	assert (key < MAX_EGKEYS);

		
	#if defined(USE_FD)
		if (NULL == (finp = egkey[key].fd) ) {
			if (NULL == (finp = fd_openit (key))) {
				return FALSE;
			}	
		}
	#else
		sprintf (buf, "%s.gtb", egkey[key].str);
		if (NULL == (finp = fopen (filename, "rb"))) {
			return FALSE;
		}	
	#endif

	ok = fpark_entry_packed (finp, side, maxindex, idx);
	ok = ok && fread_entry_packed (finp, side, &x);

	if (ok) {
		*out_dtm = x;		
	} else
		*out_dtm = iFORBID;

	#if !defined(USE_FD)
	fclose (finp);
	#endif

	return ok;
}



static bool_t
egtb_get_dtm (int k, unsigned stm, const SQUARE *wS, const SQUARE *bS, bool_t probe_hard_flag, dtm_t *dtm)
{
	bool_t idxavail;
	index_t index;
	dtm_t *tab[2];
	bool_t (*pc2idx) (const SQUARE *, const SQUARE *, index_t *);

	FOLLOW_label("egtb_get_dtm --> starts")

	if (egkey[k].status == STATUS_MALLOC || egkey[k].status == STATUS_STATICRAM) {

		tab[WH] = egkey[k].egt_w;
		tab[BL] = egkey[k].egt_b;
		pc2idx  = egkey[k].pctoi;

		idxavail = pc2idx (wS, bS, &index);

		FOLLOW_LU("indexavail (RAM)",idxavail)

		if (idxavail) {
			*dtm = tab[stm][index];
		} else {
			*dtm = iFORBID;
		}

		return TRUE;

	} else if (egkey[k].status == STATUS_ABSENT) {

		pc2idx   = egkey[k].pctoi;
		idxavail = pc2idx (wS, bS, &index);

		FOLLOW_LU("indexavail (HD)",idxavail)

		if (idxavail) {
			bool_t success;

			/* 
			|		LOCK 
			*-------------------------------*/
			mythread_mutex_lock (&Egtb_lock);	

			if (tbcache_is_on()) {

				success = get_dtm       (k, stm, index, dtm, probe_hard_flag);

				FOLLOW_LU("get_dtm (succ)",success)
				FOLLOW_LU("get_dtm (dtm )",*dtm)

					#if defined(DEBUG)
					if (Uncompressed) {
						assert (TB_compression == FALSE && 	decoding_scheme() == 0 && GTB_scheme == 0);
						dtm_t dtm_temp;
						bool_t ok;
						bool_t success2 = egtb_filepeek (k, stm, index, &dtm_temp);
						ok =  (success == success2) && (!success || *dtm == dtm_temp);
						if (!ok) {
							printf ("\nERROR\nsuccess1=%d sucess2=%d\n"
									"k=%d stm=%u index=%d dtm_peek=%d dtm_cache=%d\n", 
									success, success2, k, stm, index, dtm_temp, *dtm);
							fatal_error();
						}
					}
					#endif

			} else {			
				success = egtb_filepeek (k, stm, index, dtm);
			}

			mythread_mutex_unlock (&Egtb_lock);	
			/*------------------------------*\ 
			|		UNLOCK 
			*/
			

			if (success) {
				return TRUE;
			} else {
				if (probe_hard_flag) /* after probing hard and failing, no chance to succeed later */
					egkey[k].status = STATUS_REJECT;
				*dtm = iUNKNOWN;
				return FALSE;
			}

		} else {
			*dtm = iFORBID;
			return 	TRUE;
		}
		
	} else if (egkey[k].status == STATUS_REJECT) {

		FOLLOW_label("STATUS_REJECT")

		*dtm = iFORBID;
		return 	FALSE;
	} else {

		FOLLOW_label("STATUS_WRONG!")

		assert(0);
		*dtm = iFORBID;
		return 	FALSE;
	} 

} 

static void
removepiece (SQUARE *ys, SQ_CONTENT *yp, int j)
{
    int k;
	for (k = j; ys[k] != NOSQUARE; k++) {
		ys[k] = ys[k+1];
		yp[k] = yp[k+1];
	}
}

/* 
|
|	mySHARED by probe and build 
|
\*----------------------------------------------------*/

mySHARED /*@NULL@*/ FILE *
fd_openit (int key)
{	
	int 			i;
	int 			closingkey;
	FILE *			finp = NULL;
	char	 		buf[4096];
	char *			filename = buf;
	int 			start; 
	int				end;
	int				pth;
	const char *	extension;

	assert (0 <= key && key < MAX_EGKEYS);
	assert (0 <= fd.n && fd.n <= GTB_MAXOPEN);

	/* test if I reach limit of files open */
	if (fd.n == GTB_MAXOPEN) {

		/* fclose the last accessed, first in the list */
		closingkey = fd.key[0];
		finp = egkey [closingkey].fd;
		assert (finp != NULL);
		fclose (finp);
		egkey[closingkey].fd = NULL;
		finp = NULL;

		for (i = 1; i < fd.n; i++) {
			fd.key[i-1] = fd.key[i]; 		
		}
		fd.key[--fd.n] = -1;
	} 

	assert (fd.n < GTB_MAXOPEN);

	/* set proper extensions to the File */
	if (Uncompressed) {
		assert (TB_compression == FALSE && 	decoding_scheme() == 0 && GTB_scheme == 0);
		extension = ".gtb";
	} else {
		extension = Extension[decoding_scheme()];
	}

	#if 0
	/* for debugging */
	/* Scan folders to find the File*/
	printf ("N_end: %d\n", Gtbpath_end_index);
	for (pth = 0; pth < Gtbpath_end_index && Gtbpath[pth] != NULL; pth++) {
		const char *path = Gtbpath[pth];
		printf ("path available: %s\n", path);
	}
	#endif

	/* Scan folders to find the File*/
	finp = NULL;

	start = egkey[key].pathn;
	end   = Gtbpath_end_index;
	for (pth = start; NULL == finp && pth < end && Gtbpath[pth] != NULL; pth++) {
		const char *path = Gtbpath[pth];
		size_t pl = strlen(path);
		#if 0
		printf ("path available: %s\n", path);
		#endif
		if (pl == 0) {
				sprintf (buf, "%s%s%s", path, egkey[key].str, extension);
		} else {
			if (isfoldersep( path[pl-1] )) {
				sprintf (buf, "%s%s%s", path, egkey[key].str, extension);			
			} else {
				sprintf (buf, "%s%s%s%s", path, FOLDERSEP, egkey[key].str, extension);
			}
		}
		/* Finally found the file? */
		finp = fopen (filename, "rb");
	}

	/* File was found and opened */
	if (NULL != finp) {
		fd.key [fd.n++] = key;
		egkey[key].fd = finp;
		egkey[key].pathn = pth; /* remember succesful path */
		eg_was_open[key] = 1;
		return finp;
	}


	start = 0;
	end   = egkey[key].pathn;
	for (pth = start; NULL == finp && pth < end && Gtbpath[pth] != NULL; pth++) {
		const char *path = Gtbpath[pth];
		size_t pl = strlen(path);
		#if 0
		printf ("path available: %s\n", path);
		#endif
		if (pl == 0) {
				sprintf (buf, "%s%s%s", path, egkey[key].str, extension);
		} else {
			if (isfoldersep( path[pl-1] )) {
				sprintf (buf, "%s%s%s", path, egkey[key].str, extension);			
			} else {
				sprintf (buf, "%s%s%s%s", path, FOLDERSEP, egkey[key].str, extension);
			}
		}
		/* Finally found the file? */
		finp = fopen (filename, "rb");
	}


	/* File was found and opened */
	if (NULL != finp) {
		fd.key [fd.n++] = key;
		egkey[key].fd = finp;
		egkey[key].pathn = pth; /* remember succesful path */
		eg_was_open[key] = 1;
	}

	return finp;
}


mySHARED void
sortlists (SQUARE *ws, SQ_CONTENT *wp)
{
	int i, j;
	SQUARE ts;
	SQ_CONTENT tp;
	/* input is sorted */
	for (i = 0; wp[i] != NOPIECE; i++) {
		for (j = (i+1); wp[j] != NOPIECE; j++) {	
			if (wp[j] > wp[i]) {
				tp = wp[i]; wp[i] = wp[j]; wp[j] = tp;		
				ts = ws[i]; ws[i] = ws[j]; ws[j] = ts;
			}			
		}	
	}
}

mySHARED void
list_sq_copy (const SQUARE *a, SQUARE *b)
{
	while (NOSQUARE != (*b++ = *a++))
		;
}

mySHARED void
list_pc_copy (const SQ_CONTENT *a, SQ_CONTENT *b)
{
	while (NOPIECE != (*b++ = *a++))
		;
}

mySHARED dtm_t
inv_dtm (dtm_t x)
{
	unsigned mat;
	assert ( (x & iUNKNBIT) == 0);

	if (x == iDRAW || x == iFORBID)
		return x;
	
	mat = x & 3;
	if (mat == iWMATE)
		mat = iBMATE;
	else
		mat = iWMATE;

	x = (dtm_t) ((x & ~3) | mat);

	return x;
}

static const char pctoch[] = {'-','p','n','b','r','q','k'};

mySHARED bool_t
egtb_get_id (SQ_CONTENT *w, SQ_CONTENT *b, long int *id)
{

	char pcstr[2*MAX_LISTSIZE];
	SQ_CONTENT *s; 
	char *t;
	bool_t found;
	long int i;
	static long int cache_i = 0;
	
	assert (PAWN == 1 && KNIGHT == 2 && BISHOP == 3 && ROOK == 4 && QUEEN == 5 && KING == 6);

	t = pcstr; 

	s = w;
	while (NOPIECE != *s)
		*t++ = pctoch[*s++]; 		
	s = b;
	while (NOPIECE != *s)
		*t++ = pctoch[*s++]; 		

	*t = '\0';

	found = (0 == strcmp(pcstr, egkey[cache_i].str));
	if (found) {
		*id = cache_i;
		return found;		
	}
	
	for (i = 0, found = FALSE; !found && egkey[i].str != NULL; i++) {
		found = (0 == strcmp(pcstr, egkey[i].str));
	}
	if (found) {
		cache_i = *id = i - 1;
	}
	
	return found;
}

mySHARED void
list_sq_flipNS (SQUARE *s)
{
	while (*s != NOSQUARE) {
		*s ^= 070;
		s++;
	}
}

mySHARED void
unpackdist (dtm_t d, unsigned int *res, unsigned int *ply)
{
	*ply = d >> PLYSHIFT;
	*res = d & INFOMASK;
}

mySHARED dtm_t
packdist (unsigned int inf, unsigned int ply)
{
	assert (inf <= INFOMASK);
	return (dtm_t) (inf | ply << PLYSHIFT);
}

mySHARED dtm_t
adjust_up (dtm_t dist)
{
	#if 0
	static const dtm_t adding[] = {	
		0, 1<<PLYSHIFT, 1<<PLYSHIFT, 0, 
		0, 1<<PLYSHIFT, 1<<PLYSHIFT, 0
	};
	dist += adding [dist&INFOMASK];
	return dist;
	#else							
	switch (dist & INFOMASK) {
		case iWMATE:
		case iWMATEt:
		case iBMATE:
		case iBMATEt:
			dist += 1 << PLYSHIFT;
			break;
		default:			
			break;
	}
	return dist;	
	#endif
}


mySHARED dtm_t
bestx (unsigned int stm, dtm_t a, dtm_t b)
{
	unsigned int key;	
	static const unsigned int
	comparison [4] [4] = {
	 			/*draw, wmate, bmate, forbid*/
	/* draw  */ {0, 3, 0, 0},
	/* wmate */ {0, 1, 0, 0},
	/* bmate */ {3, 3, 2, 0},
	/* forbid*/ {3, 3, 3, 0}

	/* 0 = selectfirst   */
	/* 1 = selectlowest  */
	/* 2 = selecthighest */
	/* 3 = selectsecond  */
	};

	static const unsigned int xorkey [2] = {0, 3};	
	dtm_t retu[4];
	dtm_t ret = iFORBID;
	
	assert (stm == WH || stm == BL);
	assert ((a & iUNKNBIT) == 0 && (b & iUNKNBIT) == 0 );
	
	if (a == iFORBID)
		return b;
	if (b == iFORBID)
		return a;	

	retu[0] = a; /* first parameter */
	retu[1] = a; /* the lowest by default */
	retu[2] = b; /* highest by default */
	retu[3]	= b; /* second parameter */
	if (b < a) {
		retu[1] = b;
		retu[2] = a;		
	}
	
	key = comparison [a&3] [b&3] ^ xorkey[stm];
	ret = retu [key];
		
	return ret;
}


/*--------------------------------------------------------------------------*\
 |								PACKING ZONE
 *--------------------------------------------------------------------------*/

#ifdef BUILD_CODE
static unsigned char
dtm_pack (unsigned int stm, dtm_t d)
{

	unsigned int info, plies, prefx, store, moves;
	unsigned char ret;

	if (iDRAW == d || iFORBID == d) {
		return (unsigned char) (d & 0xff);
	}

	info  = (unsigned int) d & 3;
	plies = (unsigned int) d >> 3;

	if (WH == stm) {
		switch (info) {
			case iWMATE:		
						moves = (plies + 1) /2;
						store = moves - 1;
						prefx = info;
						if (store > 63) {
							store = moves - 64;
							prefx = iDRAW;							
						}
						break;				
			case iBMATE:
						moves = plies /2;
						store = moves;
						prefx = info;
						if (store > 63) {
							store = moves - 63;
							prefx = iFORBID;							
						}
						break;
			default:	
            store = 0;
            prefx = 0;
            assert(0);
						break;
		}
		ret = (unsigned char) (prefx | (store << 2));
	} else {
		assert (BL == stm);

		switch (info) {
			case iBMATE:		
						moves = (plies + 1) /2;
						store = moves - 1;
						prefx = info;
						if (store > 63) {
							store = moves - 64;
							prefx = iDRAW;							
						}
						break;				
			case iWMATE:

						if (plies == 254) {
							/* 	exception: no position in the 5-man 
							TBs needs to store 63 for iBMATE 
							it is then used to indicate iWMATE 
							when just overflows. plies = 254
							may need to store 64, which overflows
							store 63 as iBMATE then */
							store = 63;
							prefx = iDRAW;
							break;
						}

						moves = plies /2;
						store = moves;
						prefx = info;
						if (store > 63) {
							store = moves - 63;
							prefx = iFORBID;							
						}
						break;
			default:	
            store = 0;
            prefx = 0;
            assert(0);
						break;
		}
		ret = (unsigned char) (prefx | (store << 2));
	}
	return ret;	
}
#endif

mySHARED dtm_t
dtm_unpack (unsigned int stm, unsigned char packed)
{
	unsigned int info, plies, prefx, store, moves;
	dtm_t ret;
	unsigned int p = packed;

	if (iDRAW == p || iFORBID == p) {
		return (dtm_t) p;
	}
	
	info  = (unsigned int) p & 3;
	store = (unsigned int) p >> 2;

	if (WH == stm) {
		switch (info) {
			case iWMATE:		
						moves = store + 1;
						plies = moves * 2 - 1;
						prefx = info;
						break;				

			case iBMATE:
						moves = store;
						plies = moves * 2;
						prefx = info;
						break;

			case iDRAW:
						moves = store + 1 + 63;
						plies = moves * 2 - 1;
						prefx = iWMATE;						
						break;

			case iFORBID:

						moves = store + 63;
						plies = moves * 2;
						prefx = iBMATE;
						break;
			default:	
            plies = 0;
            prefx = 0;
            assert(0);
						break;

		}
		ret = (dtm_t) (prefx | (plies << 3));
	} else {
		switch (info) {
			case iBMATE:		
						moves = store + 1;
						plies = moves * 2 - 1;
						prefx = info;
						break;				

			case iWMATE:
						moves = store;
						plies = moves * 2;
						prefx = info;
						break;

			case iDRAW:

						if (store == 63) {
						/* 	exception: no position in the 5-man 
							TBs needs to store 63 for iBMATE 
							it is then used to indicate iWMATE 
							when just overflows */
							store++;

							moves = store + 63;
							plies = moves * 2;
							prefx = iWMATE;	

							break;
						}
	
						moves = store + 1 + 63;
						plies = moves * 2 - 1;
						prefx = iBMATE;						
						break;

			case iFORBID:

						moves = store + 63;
						plies = moves * 2;
						prefx = iWMATE;
						break;
			default:	
            plies = 0;
            prefx = 0;
            assert(0);
						break;

		}
		ret = (dtm_t) (prefx | (plies << 3));
	}
	return ret;	
}

#ifdef BUILD_CODE
static bool_t
fwrite_entry_packed (FILE *dest, unsigned int side, dtm_t x)
{
	unsigned char pck;
	pck = dtm_pack (side, x);
	return SLOTSIZE == fwrite (&pck, sizeof(pck), SLOTSIZE, dest);
}
#endif

mySHARED bool_t
fread_entry_packed (FILE *finp, unsigned int side, dtm_t *px)
{
	unsigned char p[SLOTSIZE];
	bool_t ok = (SLOTSIZE == fread (p, sizeof(unsigned char), SLOTSIZE, finp));
	if (ok) {
		*px = dtm_unpack (side, p[0]);
	}
	return ok;
}

mySHARED bool_t
fpark_entry_packed  (FILE *finp, int side, index_t max, index_t idx)
{
	bool_t ok;
	size_t sz = sizeof(unsigned char);	
	long int i;
	assert (side == WH || side == BL);
	assert (finp != NULL);
	assert (idx >= 0);
	i = ((long int)side * max + idx) * (long int)(sz);
	ok = (0 == fseek (finp, i, SEEK_SET));
	return ok;
}


/*----------------------------------------------------*\ 
|
|	shared by probe and build 
|
\*/

/*---------------------------------------------------------------------*\
|			GTB CACHE Implementation  ZONE
\*---------------------------------------------------------------------*/

struct gtb_bock;

typedef struct gtb_block gtb_block_t;

struct gtb_block {
	int 			key;
	int				side;
	index_t 		offset;
	dtm_t			*p_arr;
	gtb_block_t		*prev;
	gtb_block_t		*next;
};

struct cache_table {
	bool_t			cached;
	uint64_t		hard;
	uint64_t		soft;
	uint64_t		hardmisses;
	uint64_t		hits;
	uint64_t		softmisses;
	uint64_t 		comparisons;
	size_t			max_blocks;
	size_t 			entries_per_block;
	gtb_block_t		*top;
	gtb_block_t 	*bot;
	size_t			n;
	gtb_block_t 	*entry;
	dtm_t			*buffer;
};

struct cache_table 		cachetab = {FALSE,0,0,0,0,0,0,0,0,NULL,NULL,0,NULL,NULL};

static void 		split_index (size_t entries_per_block, index_t i, index_t *o, index_t *r);
static gtb_block_t *point_block_to_replace (void);
static bool_t 		preload_cache (int key, int side, index_t idx);
static void			movetotop (gtb_block_t *t);

/*--------------------------------------------------------------------------*/

bool_t
tbcache_init (size_t cache_mem)
{
	unsigned int 	i;
	gtb_block_t 	*p;
	size_t 			entries_per_block;
	size_t 			max_blocks;
	size_t block_mem = 32 * 1024; /* 32k fixed, needed for the compression schemes */

	if (TBCACHE_INITIALIZED)
		tbcache_done();

	entries_per_block 	= block_mem / sizeof(dtm_t);
	block_mem 			= entries_per_block * sizeof(dtm_t);
	max_blocks 			= cache_mem / block_mem;
	cache_mem 			= max_blocks * block_mem;


	tbcache_reset_counters ();

	cachetab.entries_per_block 	= entries_per_block;
	cachetab.max_blocks 		= max_blocks;
	cachetab.cached 			= TRUE;
	cachetab.top 				= NULL;
	cachetab.bot 				= NULL;
	cachetab.n 					= 0;

	if (NULL == (cachetab.buffer = malloc (cache_mem))) {
		cachetab.cached = FALSE;
		return FALSE;
	}

	if (NULL == (cachetab.entry = malloc (max_blocks * sizeof(gtb_block_t)))) {
		cachetab.cached = FALSE;
		free (cachetab.buffer);
		return FALSE;
	}

	
	for (i = 0; i < max_blocks; i++) {
		
		p = &cachetab.entry[i];
		p->key  = -1;
		p->side = -1;
		p->offset = -1;
		p->p_arr = cachetab.buffer + i * entries_per_block;
		p->prev = NULL;
		p->next = NULL;
	}


	TBCACHE_INITIALIZED = TRUE;
	return TRUE;
}

void
tbcache_done (void)
{
	assert(TBCACHE_INITIALIZED);

	cachetab.cached = FALSE;
	cachetab.hard = 0;
	cachetab.soft = 0;
	cachetab.hardmisses = 0;
	cachetab.hits = 0;
	cachetab.softmisses = 0;
	cachetab.comparisons = 0;
	cachetab.max_blocks = 0;
	cachetab.entries_per_block = 0;

	cachetab.top = NULL;
	cachetab.bot = NULL;
	cachetab.n = 0;

	if (cachetab.buffer != NULL)
		free (cachetab.buffer);
	cachetab.buffer = NULL;

	if (cachetab.entry != NULL)
		free (cachetab.entry);
	cachetab.entry = NULL;

	TBCACHE_INITIALIZED = FALSE;
	return;
}

/*
static double
tbcache_efficiency (void)
{ 
	statcounter_t tot = cachetab.hits + cachetab.hardmisses;
	return tot==0? 0: (double)1.0 * cachetab.hits / tot;
}
*/

static void
tbcache_reset_counters (void)
{
	cachetab.hard = 0;
	cachetab.soft = 0;
	cachetab.hardmisses = 0;
	cachetab.hits = 0;
	cachetab.softmisses = 0;
	cachetab.comparisons = 0;
	return;
}


extern bool_t
tbcache_is_on (void)
{
	return cachetab.cached;
}

/* STATISTICS OUTPUT */

extern void tbstats_get (struct TB_STATS *x)
{
	uint64_t hh,hm,sh,sm;
	long unsigned mask = 0xfffffffflu;

	hm = cachetab.hardmisses;
	hh = cachetab.hard - cachetab.hardmisses;
	sm = cachetab.softmisses;
	sh = cachetab.soft - cachetab.softmisses;


	x->probe_hard_hits[0] = (long unsigned)(hh & mask);
	x->probe_hard_hits[1] = (long unsigned)(hh >> 32);

	x->probe_hard_miss[0] = (long unsigned)(hm & mask);
	x->probe_hard_miss[1] = (long unsigned)(hm >> 32);

	x->probe_soft_hits[0] = (long unsigned)(sh & mask);
	x->probe_soft_hits[1] = (long unsigned)(sh >> 32);

	x->probe_soft_miss[0] = (long unsigned)(sm & mask);
	x->probe_soft_miss[1] = (long unsigned)(sm >> 32);

	x->bytes_read[0]	  = (long unsigned)(Bytes_read & mask);
	x->bytes_read[1] 	  = (long unsigned)(Bytes_read >> 32);

	x->files_opened = eg_was_open_count();

	x->blocks_occupied = cachetab.n;
	x->blocks_max      = cachetab.max_blocks;	
	x->comparisons     = cachetab.comparisons;

}

extern void	tbstats_reset (void)
{
	tbcache_reset_counters ();
	eg_was_open_reset();
}


/****************************************************************************\
|
|
|			WRAPPERS for ENCODING/DECODING FUNCTIONS ZONE
|
|
\****************************************************************************/

#include "decode.h"

/*
|
|	PRE LOAD CACHE AND DEPENDENT FUNCTIONS 
|
\*--------------------------------------------------------------------------*/

struct ZIPINFO {
	int 		extraoffset;
	int 		totalblocks;
	index_t *	blockindex;
};

struct ZIPINFO Zipinfo[MAX_EGKEYS];

static index_t 	egtb_block_getnumber 		(int key, int side, index_t idx);
static index_t 	egtb_block_getsize 			(int key, index_t idx);
static index_t 	egtb_block_getsize_zipped 	(int key, index_t block );
static bool_t 	egtb_block_park  			(int key, index_t block);
static bool_t 	egtb_block_read 			(int key, index_t len, unsigned char *buffer); 
static bool_t 	egtb_block_decode 			(int key, int z, unsigned char *bz, int n, unsigned char *bp);
static bool_t 	egtb_block_unpack 			(int side, index_t n, const unsigned char *bp, dtm_t *out);
static bool_t 	egtb_file_beready 			(int key);
static bool_t 	egtb_loadindexes 			(int key);
static index_t 	egtb_block_uncompressed_to_index (int key, index_t b);
static bool_t 	fread32 					(FILE *f, unsigned long int *y);


static int
zipinfo_init (void)
{
	int i, start, end, ret;
	bool_t ok, ok3, ok4, ok5;

	/* reset all values */
	for (i = 0; i < MAX_EGKEYS; i++) {
		Zipinfo[i].blockindex = NULL;
	 	Zipinfo[i].extraoffset = 0;
	 	Zipinfo[i].totalblocks = 0;
	}

	/* load all values */
	start = 0;
	end   = 5;
	for (i = start, ok = TRUE; i < end; i++) {
		ok = NULL != fd_openit(i);
		ok = ok && egtb_loadindexes (i);
	}
	ok3 = ok;

	start = 5;
	end   = 35;
	for (i = start, ok = TRUE; i < end; i++) {
		ok = NULL != fd_openit(i);
		ok = ok && egtb_loadindexes (i);
	}
	ok4 = ok;

	start = 35;
	end   = MAX_EGKEYS;
	for (i = start, ok = TRUE; i < end; i++) {
		ok = NULL != fd_openit(i);
		ok = ok && egtb_loadindexes (i);
	}
	ok5 = ok;

	ret = 0;
	if (ok3) {
		ret = 3;	
		if (ok4) {
			ret = 4;
			if (ok5) 
				ret = 5;
		}
	}

	return ret;
}

static void
zipinfo_done (void)
{
	int i;
	bool_t ok;
	for (i = 0, ok = TRUE; ok && i < MAX_EGKEYS; i++) {
		if (Zipinfo[i].blockindex != NULL) {
			free(Zipinfo[i].blockindex);
			Zipinfo[i].blockindex = NULL;
		 	Zipinfo[i].extraoffset = 0;
		 	Zipinfo[i].totalblocks = 0;
		}
	}
	return;
}

static bool_t
fread32 (FILE *f, unsigned long int *y)
{
	enum SIZE {SZ = 4};
	int i;
	unsigned long int x;
	unsigned char p[SZ];
	bool_t ok;

	ok = (SZ == fread (p, sizeof(unsigned char), SZ, f));

	if (ok) {
		for (x = 0, i = 0; i < SZ; i++) {
			x |= (unsigned long int)p[i] << (i*8);
		}
		*y = x;
	}
	return ok;
}

static bool_t
egtb_loadindexes (int key)
{
	unsigned long int blocksize = 1;
	unsigned long int tailblocksize1 = 0;
	unsigned long int tailblocksize2 = 0;
    unsigned long int offset=0;
	unsigned long int dummy;
	unsigned long int i;
	unsigned long int blocks;
	unsigned long int idx;
	unsigned long int n_idx;
	index_t	*p;

	bool_t ok;

	FILE *f;

	if (Uncompressed) {
		assert (TB_compression == FALSE && 	decoding_scheme() == 0 && GTB_scheme == 0);	
		return TRUE; /* no need to load indexes */
	}
	if (Zipinfo[key].blockindex != NULL)
		return TRUE; /* indexes must have been loaded already */

	if (NULL == (f = egkey[key].fd))
		return FALSE; /* file was no open */

	/* Get Reserved bytes, blocksize, offset */
	ok = (0 == fseek (f, 0, SEEK_SET)) &&
	fread32 (f, &dummy) &&	
	fread32 (f, &dummy) &&
	fread32 (f, &blocksize) &&
	fread32 (f, &dummy) &&
	fread32 (f, &tailblocksize1) &&
	fread32 (f, &dummy) &&
	fread32 (f, &tailblocksize2) &&
	fread32 (f, &dummy) &&
	fread32 (f, &offset) &&
	fread32 (f, &dummy);

	blocks = (offset - 40)/4 -1;
	n_idx = blocks + 1;

	p = NULL;

	ok = ok && NULL != (p = malloc (n_idx * 4));

	/* Input of Indexes */
	for (i = 0; ok && i < n_idx; i++) {
		ok = fread32 (f, &idx);
		p[i] = idx;
	}

	if (ok) {
		Zipinfo[key].extraoffset = 0;	
		Zipinfo[key].totalblocks = n_idx;
		Zipinfo[key].blockindex  = p;
	}	

	if (!ok && p != NULL) {
		free(p);
	}

	return ok;
}

static index_t
egtb_block_uncompressed_to_index (int key, index_t b)
{
	index_t max;
	index_t blocks_per_side;
	index_t idx;

	max = egkey[key].maxindex;
	blocks_per_side = 1 + (max-1) / cachetab.entries_per_block;

	if (b < blocks_per_side) {
		idx = 0;
	} else {
		b -= blocks_per_side;
		idx = max;
	}
	idx += b * cachetab.entries_per_block;
	return idx;
}

static index_t
egtb_block_getnumber (int key, int side, index_t idx)
{
	index_t blocks_per_side, block_in_side;
	index_t max = egkey[key].maxindex;

	blocks_per_side = 1 + (max-1) / cachetab.entries_per_block;
	block_in_side   = idx / cachetab.entries_per_block;

	#if 0
	printf ("Inside egtb_block_getnumber\n");
	printf ("key=%lu, side=%lu, idx=%lu, cachetab.entries_per_block=%lu, max=%lu, blocks_per_side=%lu, block_in_side=%lu\n", 
			(unsigned long)key, (unsigned long)side, (unsigned 	long)idx, (unsigned long)cachetab.entries_per_block, 
			(unsigned long)max, (unsigned long)blocks_per_side, (unsigned long)block_in_side);
	#endif

	return side * blocks_per_side + block_in_side; /* block */
}

static index_t 
egtb_block_getsize (int key, index_t idx)
{
	index_t blocksz = cachetab.entries_per_block;
	index_t maxindex  = egkey[key].maxindex;
	index_t block, offset, x; 

	assert (0 <= idx && idx < maxindex);
	assert (key < MAX_EGKEYS);

	block = idx / blocksz;
	offset = block * blocksz;

	/* adjust block size in case that this is the last block 
		and is shorter than "blocksz" */
	if ( (offset + blocksz) > maxindex) 
		x = maxindex - offset; /* last block size */
	else
		x = blocksz; /* size of a normal block */
	
	return x;
}

static index_t 
egtb_block_getsize_zipped (int key, index_t block )
{
	index_t i, j;
	assert (Zipinfo[key].blockindex != NULL);
	i = Zipinfo[key].blockindex[block];
	j = Zipinfo[key].blockindex[block+1];	
	return j - i;
}


static bool_t
egtb_file_beready (int key)
{
	bool_t success;
	assert (key < MAX_EGKEYS);
	success = 	(NULL != egkey[key].fd) ||
				(NULL != fd_openit(key) && egtb_loadindexes (key));
	return success; 
}


static bool_t
egtb_block_park  (int key, index_t block)
{
	long int i;
	assert (egkey[key].fd != NULL);

	if (Uncompressed) {
		assert (TB_compression == FALSE && 	decoding_scheme() == 0 && GTB_scheme == 0);	
		i = egtb_block_uncompressed_to_index (key, block);
	} else {
		assert (Zipinfo[key].blockindex != NULL);
		i  = Zipinfo[key].blockindex[block];
		i += Zipinfo[key].extraoffset;
	}

	return 0 == fseek (egkey[key].fd, i, SEEK_SET);
}


static bool_t
egtb_block_read (int key, index_t len, unsigned char *buffer) 
{
	assert (egkey[key].fd != NULL);
	return ((size_t)len == fread (buffer, sizeof (unsigned char), len, egkey[key].fd));	
}

int __indexing_dummy;

static bool_t
egtb_block_decode (int key, int z, unsigned char *bz, int n, unsigned char *bp)
/* bz:buffer zipped to bp:buffer packed */
{
	__indexing_dummy = key; /* to silence compiler */	
	return decode (z-1, bz+1, n, bp);

}

static bool_t
egtb_block_unpack (int side, index_t n, const unsigned char *bp, dtm_t *out)
/* bp:buffer packed to out:distance to mate buffer */
{
	int i;
	for (i = 0; i < n; i++) {
		*out++ = dtm_unpack (side, bp[i]);		
	}	
	return TRUE;
}

static bool_t
preload_cache (int key, int side, index_t idx)
/* output to the least used block of the cache */
{
	gtb_block_t 	*pblock;
	dtm_t 			*p;
	bool_t 			ok;

	FOLLOW_label("preload_cache starts")

	if (idx >= egkey[key].maxindex) {
		FOLLOW_LULU("Wrong index", __LINE__, idx)	
		return FALSE;
	}
	
	/* find aged blocked in cache */
	pblock = point_block_to_replace();
	p = pblock->p_arr;

	if (Uncompressed) {

		index_t block = egtb_block_getnumber (key, side, idx);
		int n         = egtb_block_getsize   (key, idx);

		ok =	   egtb_file_beready (key)
				&& egtb_block_park   (key, block)
				&& egtb_block_read   (key, n, Buffer_packed)
				&& egtb_block_unpack (side, n, Buffer_packed, p);	

		FOLLOW_LULU("preload_cache", __LINE__, ok)

		assert (TB_compression == FALSE && 	decoding_scheme() == 0 && GTB_scheme == 0);	

		if (ok) Bytes_read += n;

	} else {


        index_t block=0;
        int n=0, z=0;
		
		ok =	   egtb_file_beready (key);

		FOLLOW_LULU("preload_cache", __LINE__, ok)

		if (ok) {
			block = egtb_block_getnumber (key, side, idx);
			n     = egtb_block_getsize   (key, idx);
			z     = egtb_block_getsize_zipped (key, block);				
		}

		ok =	   ok
				&& egtb_block_park   (key, block);
		FOLLOW_LULU("preload_cache", __LINE__, ok)

		ok =	   ok	
				&& egtb_block_read   (key, z, Buffer_zipped);
		FOLLOW_LULU("preload_cache", __LINE__, ok)

		ok =	   ok		
				&& egtb_block_decode (key, z, Buffer_zipped, n, Buffer_packed);
		FOLLOW_LULU("preload_cache", __LINE__, ok)

		ok =	   ok		 
				&& egtb_block_unpack (side, n, Buffer_packed, p);	
		FOLLOW_LULU("preload_cache", __LINE__, ok)

		if (ok) Bytes_read += z;
	}

	if (ok) {

		index_t 		offset;
		index_t			remainder;
		split_index (cachetab.entries_per_block, idx, &offset, &remainder); 

		pblock->key    = key;
		pblock->side   = side;
		pblock->offset = offset;
	} else {
		/* make it unusable */
		pblock->key    = -1;
		pblock->side   = -1;
		pblock->offset = -1;
	}

	FOLLOW_LU("preload_cache?", ok)

	return ok;		
}

/****************************************************************************\
|
|
|						MEMORY ALLOCATION ZONE
|
|
\****************************************************************************/


mySHARED void
egtb_freemem (int i)
{
	if (egkey[i].status == STATUS_MALLOC) {
		assert (egkey[i].egt_w != NULL);
		assert (egkey[i].egt_b != NULL);	
		free (egkey[i].egt_w);
		free (egkey[i].egt_b);	
		egkey[i].egt_w = NULL;
		egkey[i].egt_b = NULL;
	}	
	egkey[i].status = STATUS_ABSENT;	
}


#ifdef BUILD_CODE
static bool_t
egtb_reservemem (int key, long int n, size_t sz)
{
	if (NULL == egkey[key].egt_w) {
		dtm_t *p;
		dtm_t *q;
		assert (NULL == egkey[key].egt_b);
		p = malloc (n * sz);
		q = malloc (n * sz);
		if (NULL == p || NULL == q) {
			fprintf (stderr, "Lack of memory error\n");
			if (NULL != p) free (p);
			if (NULL != q) free (q);			
			return FALSE;
		}
		egkey[key].egt_w = p;
		egkey[key].egt_b = q;
	}

	assert (NULL != egkey[key].egt_b);
	return TRUE;
}
#endif

/***************************************************************************/

#if 0
static dtm_t
bettercontent_for_first_isbetter (unsigned stm, dtm_t a, dtm_t b)
{
	int key;
	unsigned ret;
	unsigned uncbit;
	
	assert (stm == WH || stm == BL);
	#if 1
	key = bettarr [stm] [a & INFOMASK] [b & INFOMASK];
	#else
	key = bettarr [stm] [a &  RESMASK] [b &  RESMASK];		/* comparison without uncbits */
	#endif
	
	uncbit = (a | b) & iUNKNBIT;

	if (a == iUNKNOWN)
		return (dtm_t) (b | uncbit);

	if (b == iUNKNOWN)
		return (dtm_t) (a | uncbit);

	switch (key) {
		case 1: ret = a; break;
		case 2: ret = b; break;
		case 3: ret = a < b? a: b; break;
		case 4: ret = a > b? a: b; break;
		default: ret = 0; printf("%d\n",key); assert(0); break;											
	}
	return (dtm_t) (ret | uncbit);
}


extern bool_t
egtb_first_isbetter (unsigned int stm, unsigned a_inf, unsigned a_ply,
									unsigned b_inf, unsigned b_ply)
{
	dtm_t a = packdist (a_inf, a_ply);
	dtm_t b = packdist (b_inf, b_ply);
	assert (stm == WH || stm == BL);
	return (a == bettercontent_for_first_isbetter (stm, a, b));
}
#endif

/*===========================================================================*/	

#ifdef BUILD_CODE

static dtm_t
epcapture_score (unsigned int capturingside, int i, int j, SQUARE epsq,
				 SQUARE      *xs,  SQUARE      *ys, 
				 SQ_CONTENT  *xp,  SQ_CONTENT  *yp, 
				 SQUARE     *wSQ,  SQUARE     *bSQ, 
				 SQ_CONTENT *wPC,  SQ_CONTENT *bPC, 
				dtm_t *score
				);

static bool_t
egtb_present (int key)
{
	/* get file descriptor */
	if (NULL == egkey[key].fd) {
		if (NULL == fd_openit (key) ) {
			return FALSE;
		}	
	} 
	/* do not need to close file opened by fd_openit: it will be done at egtb_done() */
	return TRUE;
}

static bool_t
to_fourth (unsigned int stm, SQUARE to)
{
	static const unsigned flipcode [2] = {0, 070};
	assert (stm == WH || stm == BL);	
	assert (A2 <= to && to < A8);	
	return 3 == ((to^flipcode[stm]) >> 3);
}

static dtm_t
epcapture_score (unsigned int capturingside, int i, int j, SQUARE epsq,
				 SQUARE      *xs,  SQUARE      *ys, 
				 SQ_CONTENT  *xp,  SQ_CONTENT  *yp, 
				 SQUARE     *wSQ,  SQUARE     *bSQ, 
				 SQ_CONTENT *wPC,  SQ_CONTENT *bPC, 
				dtm_t *score
				)
{
	int k;
	SQUARE 		tmp_xs[MAX_LISTSIZE], tmp_ys[MAX_LISTSIZE];
	SQ_CONTENT	tmp_xp[MAX_LISTSIZE], tmp_yp[MAX_LISTSIZE];
	unsigned int stm;
	bool_t skip;
	dtm_t dist;
	unsigned int inf;
	unsigned int plies;
		
	/* temp store */
	list_sq_copy (xs, tmp_xs);
	list_sq_copy (ys, tmp_ys);
	list_pc_copy (xp, tmp_xp);
	list_pc_copy (yp, tmp_yp);

	/*capture from j to i */
	ys[j] = epsq; /* captured square */
				
	for (k = i; xs[k] != NOSQUARE; k++) {
		xs[k] = xs[k+1];
		xp[k] = xp[k+1];
	}

	stm = Opp(capturingside);
	
	skip = FALSE;
	
	dist = iUNKNOWN; /* just to improve debugging */
			
	if (tb_probe_hard (stm, NOSQUARE, wSQ, bSQ, wPC, bPC, &inf, &plies) ) {
		dist = packdist (inf, plies); 
		/* legal capture ?*/
		skip = dist == iFORBID;
	} else {
		/* absent egtb */
		fprintf(stderr, "absent egtb\n");
		output_state (stm, wSQ, bSQ, wPC, bPC);
		assert(0);
		fatal_error();
	}
					
	if (!skip) {
		assert (0 == (dist & iUNKNBIT));
		assert ((dist & INFOMASK) < 3);
		dist = adjust_up (dist);
	} /* not skip */

	/* restore original */
	list_sq_copy (tmp_xs, xs);
	list_sq_copy (tmp_ys, ys);
	list_pc_copy (tmp_xp, xp);
	list_pc_copy (tmp_yp, yp);		

	*score = dist;
	
	return TRUE;
	
}
#endif

/*--------------------------------------------------------------------------------*/

mySHARED bool_t
get_dtm (int key, int side, index_t idx, dtm_t *out, bool_t probe_hard_flag)
{
	bool_t found;

	if (probe_hard_flag) {
		cachetab.hard++;
	} else {
		cachetab.soft++;
	}

	if (get_dtm_from_cache (key, side, idx, out)) {
		cachetab.hits++;
		found = TRUE;
	} else if (probe_hard_flag) {
		cachetab.hardmisses++;
		found = preload_cache (key, side, idx) &&
				get_dtm_from_cache (key, side, idx, out);
	} else {
		cachetab.softmisses++;
		found = FALSE;
	}
	return found;
}


static bool_t
get_dtm_from_cache (int key, int side, index_t idx, dtm_t *out)
{
	index_t 	offset;
	index_t		remainder;
	bool_t 		found;
	gtb_block_t	*p;

	if (!TB_cache_on)
		return FALSE;

	split_index (cachetab.entries_per_block, idx, &offset, &remainder); 

	found = FALSE;
	for (p = cachetab.top; p != NULL; p = p->prev) {

		cachetab.comparisons++;

		if (   key     == p->key 
			&& side    == p->side 
			&& offset  == p->offset) {

			found = TRUE;
			*out = p->p_arr[remainder];
			break;
		}
	}

	if (found) {
		movetotop(p);
	}

	FOLLOW_LU("get_dtm_from_cache ok?",found)

	return found;
}


static void
split_index (size_t entries_per_block, index_t i, index_t *o, index_t *r)
{
	index_t n;
	n  = i / (index_t) entries_per_block;
	*o = n * (index_t) entries_per_block;
	*r = i - *o;
	return;
}


static gtb_block_t *
point_block_to_replace (void)
{
	gtb_block_t *p, *t, *s;

	assert (0 == cachetab.n || cachetab.top != NULL);
	assert (0 == cachetab.n || cachetab.bot != NULL);
	assert (0 == cachetab.n || cachetab.bot->prev == NULL);
	assert (0 == cachetab.n || cachetab.top->next == NULL);

	if (cachetab.n > 0 && -1 == cachetab.top->key) {

		/* top entry is unusable, should be the one to replace*/
		p = cachetab.top;

	} else
	if (cachetab.n == 0) {
		
		p = &cachetab.entry[cachetab.n++];
		cachetab.top = p;
		cachetab.bot = p;
	
		p->prev = NULL;
		p->next = NULL;

	} else
	if (cachetab.n < cachetab.max_blocks) { /* add */

		s = cachetab.top;
		p = &cachetab.entry[cachetab.n++];
		cachetab.top = p;
	
		s->next = p;
		p->prev = s;
		p->next = NULL;

	} else {                       /* replace*/ 
		
		t = cachetab.bot;
		s = cachetab.top;
		cachetab.bot = t->next;
		cachetab.top = t;
		
		s->next = t;
		t->prev = s;
		cachetab.top->next = NULL;
		cachetab.bot->prev = NULL;

		p = t;
	}
	
	/* make the information content unusable, it will be replaced */
	p->key    = -1;
	p->side   = -1;
	p->offset = -1;

	return p;
}

static void
movetotop (gtb_block_t *t)
{
	gtb_block_t *s, *nx, *pv;

	assert (t != NULL);

	if (t->next == NULL) /* at the top already */
		return;

	/* detach */
	pv = t->prev;
	nx = t->next;

	if (pv == NULL)  /* at the bottom */
		cachetab.bot = nx;
	else 
		pv->next = nx;

	if (nx == NULL) /* at the top */
		cachetab.top = pv;
	else
		nx->prev = pv;

	/* relocate */
	s = cachetab.top;
	assert (s != NULL);
	if (s == NULL)
		cachetab.bot = t;	
	else
		s->next = t;

	t->next = NULL;
	t->prev = s;
	cachetab.top = t;

	return;
}

/****************************************************************************\
 *
 *
 *								INDEXING ZONE
 *
 *
 ****************************************************************************/

static void
init_indexing (int verbosity)
{
	int a,b,c,d,e,f;

	init_flipt ();

	a = init_kkidx     () ;	
	b = init_ppidx     () ;	
	c = init_aaidx     () ;
	d = init_aaa       () ;
	e = init_pp48_idx  () ;
	f = init_ppp48_idx () ;

	if (verbosity) {
		printf ("\nGTB supporting tables, Initialization\n");
		printf ("  Max    kk idx: %8d\n", a );	
		printf ("  Max    pp idx: %8d\n", b );	
		printf ("  Max    aa idx: %8d\n", c );
		printf ("  Max   aaa idx: %8d\n", d );
		printf ("  Max  pp48 idx: %8d\n", e );
		printf ("  Max ppp48 idx: %8d\n", f );
	}

	if (!reach_was_initialized())
		reach_init();

	/* testing */

	if (0) {
		list_index ();
		printf ("\nTEST indexing functions\n");

		test_kaakb ();
		test_kaabk ();
		test_kaaak ();
		test_kabbk ();
	
		test_kapkb ();
		test_kabkp ();
	
		test_kppka ();

		test_kapkp ();
		test_kabpk();
		test_kaapk ();
	
		test_kappk ();	
		test_kaakp ();
		test_kppk ();
	 	test_kppkp ();
	 	test_kpppk ();
	}	
	return;
}


static index_t
init_kkidx (void)
/* modifies kkidx[][], wksq[], bksq[] */
{
	int index;
	SQUARE x, y, i, j;
	
	/* default is noindex */
	for (x = 0; x < 64; x++) {
		for (y = 0; y < 64; y++) {
			kkidx [x][y] = NO_KKINDEX;
		}
	}

	index = 0;
	for (x = 0; x < 64; x++) {
		for (y = 0; y < 64; y++) {
		
			/* is x,y illegal? continue */
			if (possible_attack (x, y, wK) || x == y)
				continue;
		
			/* normalize */
			/*i <-- x; j <-- y */
			norm_kkindex (x, y, &i, &j);
		
			if (NO_KKINDEX == kkidx [i][j]) { /* still empty */
				kkidx [i][j] = index;
				kkidx [x][y] = index;
				bksq [index] = i;
				wksq [index] = j;			
				index++;
			}
		}
	}
	
	assert (index == MAX_KKINDEX);

	return index;
}


static index_t
init_aaidx (void)
/* modifies aabase[], aaidx[][] */
{
	int index;
	SQUARE x, y;
	
	/* default is noindex */
	for (x = 0; x < 64; x++) {
		for (y = 0; y < 64; y++) {
			aaidx [x][y] = NOINDEX;
		}
	}

	for (index = 0; index < MAX_AAINDEX; index++)
		aabase [index] = 0;

	index = 0;
	for (x = 0; x < 64; x++) {
		for (y = x + 1; y < 64; y++) {

			assert (index == (int)((y - x) + x * (127-x)/2 - 1) );

			if (NOINDEX == aaidx [x][y]) { /* still empty */
				aaidx [x] [y] = index; 
				aaidx [y] [x] = index;
				aabase [index] = (unsigned char) x;
				index++;
			} else {
				assert (aaidx [x] [y] == index);
				assert (aabase [index] == x);
			}


		}
	}
	
	assert (index == MAX_AAINDEX);

	return index;
}


static index_t
init_ppidx (void)
/* modifies ppidx[][], pp_hi24[], pp_lo48[] */
{
	int i, j;
	int idx = 0;
	SQUARE a, b;

	/* default is noindex */
	for (i = 0; i < 24; i++) {
		for (j = 0; j < 48; j++) {
			ppidx [i][j] = NOINDEX;
		}
	}
		
	for (idx = 0; idx < MAX_PPINDEX; idx++) {
		pp_hi24 [idx] = NOINDEX;	
		pp_lo48 [idx] =	NOINDEX;			
	}		
		
	idx = 0;
	for (a = H7; a >= A2; a--) {

		if ((a & 07) < 4) /* square in the queen side */
			continue;

		for (b = a - 1; b >= A2; b--) {
			
			SQUARE anchor, loosen;
	
			pp_putanchorfirst (a, b, &anchor, &loosen);
			
			if ((anchor & 07) > 3) { /* square in the king side */
				anchor = flipWE(anchor);
				loosen = flipWE(loosen);				
			}
			
			i = wsq_to_pidx24 (anchor);
			j = wsq_to_pidx48 (loosen);
			
			if (ppidx [i] [j] == NOINDEX) {

                ppidx [i] [j] = idx;
                assert (idx < MAX_PPINDEX);
                pp_hi24 [idx] = i;
                assert (i < 24);
                pp_lo48 [idx] =	j;
                assert (j < 48);
				idx++;
			}
			
		}	
	}
	assert (idx == MAX_PPINDEX);
	return idx;
}

static void
init_flipt (void)
{
	int i, j;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 64; j++) {
			flipt [i] [j] = flip_type (i, j);
		}		
	}
}

/*--- NORMALIZE -------*/

static void
norm_kkindex (SQUARE x, SQUARE y, /*@out@*/ SQUARE *pi, /*@out@*/ SQUARE *pj)
{
	unsigned int rowx, rowy, colx, coly;
	
	assert (x < 64);
	assert (y < 64);
	
	if (getcol(x) > 3) { 
		x = flipWE (x); /* x = x ^ 07  */
		y = flipWE (y);		
	}
	if (getrow(x) > 3)  { 
		x = flipNS (x); /* x = x ^ 070  */
		y = flipNS (y);		
	}	
	rowx = getrow(x);
	colx = getcol(x);
	if ( rowx > colx ) {
		x = flipNW_SE (x); /* x = ((x&7)<<3) | (x>>3) */
		y = flipNW_SE (y);			
	}
	rowy = getrow(y);
	coly = getcol(y);	
	if ( rowx == colx && rowy > coly) {
		x = flipNW_SE (x);
		y = flipNW_SE (y);			
	}	
	
	*pi = x;
	*pj = y;
}

static unsigned int
flip_type (SQUARE x, SQUARE y)
{
	unsigned int rowx, rowy, colx, coly;
	unsigned int ret = 0;
	
	assert (x < 64);
	assert (y < 64);
	
	
	if (getcol(x) > 3) { 
		x = flipWE (x); /* x = x ^ 07  */
		y = flipWE (y);		
		ret |= 1;
	}
	if (getrow(x) > 3)  { 
		x = flipNS (x); /* x = x ^ 070  */
		y = flipNS (y);		
		ret |= 2;		
	}	
	rowx = getrow(x);
	colx = getcol(x);
	if ( rowx > colx ) {
		x = flipNW_SE (x); /* x = ((x&7)<<3) | (x>>3) */
		y = flipNW_SE (y);	
		ret |= 4;				
	}
	rowy = getrow(y);
	coly = getcol(y);	
	if ( rowx == colx && rowy > coly) {
		x = flipNW_SE (x);
		y = flipNW_SE (y);	
		ret |= 4;					
	}	
	return ret;
}


static void
pp_putanchorfirst (SQUARE a, SQUARE b, /*@out@*/ SQUARE *out_anchor, /*@out@*/ SQUARE *out_loosen)
{
	unsigned int anchor, loosen;
			
	unsigned int row_b, row_a;
	row_b = b & 070;
	row_a = a & 070;
			
	/* default */
	anchor = a;
	loosen = b;
	if (row_b > row_a) {
		anchor = b;
		loosen = a;
	} 
	else
	if (row_b == row_a) {
		unsigned int x, col, inv, hi_a, hi_b;
		x = a;
		col = x & 07;
		inv = col ^ 07;
		x = (1<<col) | (1<<inv);
		x &= (x-1);	
		hi_a = x;
		
		x = b;
		col = x & 07;
		inv = col ^ 07;
		x = (1<<col) | (1<<inv);
		x &= (x-1);	
		hi_b = x;			
				
		if (hi_b > hi_a) {
			anchor = b;
			loosen = a;					
		}

		if (hi_b < hi_a) {
			anchor = a;
			loosen = b;					
		}

		if (hi_b == hi_a) {
			if (a < b) {
				anchor = a;
				loosen = b;	
			} else {
				anchor = b;
				loosen = a;	
			}				
		}
	}

	*out_anchor = anchor;
	*out_loosen = loosen;
	return;
}


static SQUARE
wsq_to_pidx24 (SQUARE pawn)
{
	unsigned int idx24;
	SQUARE sq = pawn;

	/* input can be only queen side, pawn valid */
	assert (A2 <= pawn && pawn < A8);
	assert ((pawn & 07) < 4);

	sq ^= 070; /* flipNS*/
	sq -= 8;   /* down one row*/
	idx24 = (sq+(sq&3)) >> 1; 
	assert (idx24 < 24);
	return idx24;
}

static SQUARE
wsq_to_pidx48 (SQUARE pawn)
{
	unsigned int idx48;
	SQUARE sq = pawn;

	/* input can be both queen or king side, pawn valid square  */
	assert (A2 <= pawn && pawn < A8);

	sq ^= 070; /* flipNS*/
	sq -= 8;   /* down one row*/
	idx48 = sq;
	assert (idx48 < 48);
	return idx48;
}

static SQUARE
pidx24_to_wsq (unsigned int a)
{
	enum  {B11100  = 7u << 2};
	unsigned int x;
	assert (a < 24);
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
	x ^= 070;        /* flip NS */
	return x;
}

static SQUARE
pidx48_to_wsq (unsigned int a)
{
	unsigned int x;
	assert (a < 48);
	/* x is pslice */
	x = a;
	x += 8;          /* add extra row  */
	x ^= 070;        /* flip NS */
	return x;
}


static void
kxk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = 64}; 
	index_t a = i / BLOCK_A;
	index_t b = i - a * BLOCK_A;
	
	pw[0] = wksq [a];
	pb[0] = bksq [a];
	pw[1] = b;
	pw[2] = NOSQUARE;
	pb[1] = NOSQUARE;
	
	assert (kxk_pctoindex (pw, pb, &a) && a == i);

	return;
}

static bool_t 
kxk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {BLOCK_A = 64}; 
	SQUARE *p;
	SQUARE ws[32], bs[32];
	index_t ki;
	int i;
	
	unsigned int ft;
	
	ft = flip_type (inp_pb[0],inp_pw[0]);

	assert (ft < 8);


	for (i = 0; inp_pw[i] != NOSQUARE; i++) {
		ws[i] = inp_pw[i];
	}
	ws[i] = NOSQUARE;
	for (i = 0; inp_pb[i] != NOSQUARE; i++) {
		bs[i] = inp_pb[i];
	}
	bs[i] = NOSQUARE;

	if ((ft & 1) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipWE (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipWE (*p);
	}

	if ((ft & 2) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipNS (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipNS (*p);
	}

	if ((ft & 4) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipNW_SE (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipNW_SE (*p);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

	if (ki == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ws[1]; 
	return TRUE;
	
}


static void
kabk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	index_t a, b, c, r;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;
	
	pw[0] = wksq [a];
	pb[0] = bksq [a];

	pw[1] = b;
	pw[2] = c;
	pw[3] = NOSQUARE;

	pb[1] = NOSQUARE;
	
	assert (kabk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kabk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	SQUARE *p;
	SQUARE ws[32], bs[32];
	index_t ki;
	int i;
	
	unsigned int ft;
	
	ft = flip_type (inp_pb[0],inp_pw[0]);

	assert (ft < 8);

	for (i = 0; inp_pw[i] != NOSQUARE; i++) {
		ws[i] = inp_pw[i];
	}
	ws[i] = NOSQUARE;
	for (i = 0; inp_pb[i] != NOSQUARE; i++) {
		bs[i] = inp_pb[i];
	}
	bs[i] = NOSQUARE;

	if ((ft & 1) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipWE (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipWE (*p);
	}

	if ((ft & 2) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipNS (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipNS (*p);
	}

	if ((ft & 4) != 0) {
		for (p = ws; *p != NOSQUARE; p++) 
				*p = flipNW_SE (*p);
		for (p = bs; *p != NOSQUARE; p++) 
				*p = flipNW_SE (*p);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

	if (ki == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ws[1] * BLOCK_B + ws[2]; 
	return TRUE;
	
}


static void
kabkc_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;
	d  = r;
	
	pw[0] = wksq [a];
	pb[0] = bksq [a];

	pw[1] = b;
	pw[2] = c;
	pw[3] = NOSQUARE;

	pb[1] = d;
	pb[2] = NOSQUARE;
	
	assert (kabkc_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kabkc_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {N_WHITE = 3, N_BLACK = 2};

	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki;
	int i;
	unsigned int ft;

	#if 0
		ft = flip_type (inp_pb[0], inp_pw[0]);
	#else
		ft = flipt [inp_pb[0]] [inp_pw[0]];
	#endif

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}


	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

	if (ki == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ws[1] * BLOCK_B + ws[2] * BLOCK_C + bs[1]; 
	return TRUE;
	
}

/* ABC/ ***/

extern void
kabck_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;
	d  = r;
	
	pw[0] = wksq [a];
	pb[0] = bksq [a];

	pw[1] = b;
	pw[2] = c;
	pw[3] = d;
	pw[4] = NOSQUARE;

	pb[1] = NOSQUARE;
	
	assert (kabck_pctoindex (pw, pb, &a) && a == i);

	return;
}


extern bool_t 
kabck_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {N_WHITE = 4, N_BLACK = 1};
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 

	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki;
	int i;
	unsigned int ft;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}


	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

	if (ki == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ws[1] * BLOCK_B + ws[2] * BLOCK_C + ws[3]; 
	return TRUE;
	
}


static void
kakb_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	index_t a, b, c, r;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;
	
	pw[0] = wksq [a];
	pb[0] = bksq [a];

	pw[1] = b;
	pw[2] = NOSQUARE;

	pb[1] = c;
	pb[2] = NOSQUARE;
	
	assert (kakb_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kakb_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	SQUARE ws[32], bs[32];
	index_t ki;
	unsigned int ft;
	
	#if 0
		ft = flip_type (inp_pb[0], inp_pw[0]);
	#else
		ft = flipt [inp_pb[0]] [inp_pw[0]];
	#endif

	assert (ft < 8);

	ws[0] = inp_pw[0];
	ws[1] = inp_pw[1];
	ws[2] = NOSQUARE;
	
	bs[0] = inp_pb[0];
	bs[1] = inp_pb[1];
	bs[2] = NOSQUARE;

	if ((ft & 1) != 0) {
		ws[0] = flipWE (ws[0]);
		ws[1] = flipWE (ws[1]);
		bs[0] = flipWE (bs[0]);
		bs[1] = flipWE (bs[1]);
	}

	if ((ft & 2) != 0) {
		ws[0] = flipNS (ws[0]);
		ws[1] = flipNS (ws[1]);
		bs[0] = flipNS (bs[0]);
		bs[1] = flipNS (bs[1]);
	}

	if ((ft & 4) != 0) {
		ws[0] = flipNW_SE (ws[0]);
		ws[1] = flipNW_SE (ws[1]);
		bs[0] = flipNW_SE (bs[0]);
		bs[1] = flipNW_SE (bs[1]);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

	if (ki == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ws[1] * BLOCK_B + bs[1]; 
	return TRUE;
	
}

/********************** KAAKB *************************************/

static bool_t 	test_kaakb (void);
static bool_t 	kaakb_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kaakb_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kaakb (void)
{

	enum  {MAXPC = 16+1};
	char 		str[] = "kaakb";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = d;
			pb[1] = e;
			pb[2] = NOSQUARE;
	
			if (kaakb_pctoindex (pw, pb, &i)) {
							kaakb_indextopc (i, px, py);		
							kaakb_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kaakb_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	index_t a, b, c, r, x, y;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;

	b  = r / BLOCK_B;
	r -= b * BLOCK_B;

	c  = r;
	
	assert (i == (a * BLOCK_A + b * BLOCK_B + c));

	pw[0] = wksq [a];
	pb[0] = bksq [a];

	x = aabase [b];
	y = (b + 1) + x - (x * (127-x)/2);

	pw[1] = x;
	pw[2] = y;
	pw[3] = NOSQUARE;

	pb[1] = c;
	pb[2] = NOSQUARE;
	
	assert (kaakb_pctoindex (pw, pb, &a) && a == i);

	return;
}

static bool_t 
kaakb_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out)
{
	enum  {N_WHITE = 3, N_BLACK = 2};
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki, ai;
	unsigned int ft;
	int i;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */
	ai = aaidx [ws[1]] [ws[2]];

	if (ki == NOINDEX || ai == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ai * BLOCK_B + bs[1]; 
	return TRUE;
}

/****************** End KAAKB *************************************/

/********************** KAAB/K ************************************/

static bool_t 	test_kaabk (void);
static bool_t 	kaabk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kaabk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kaabk (void)
{

	enum  {MAXPC = 16+1};
	char 		str[] = "kaabk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kaabk_pctoindex (pw, pb, &i)) {
							kaabk_indextopc (i, px, py);		
							kaabk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kaabk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	index_t a, b, c, r, x, y;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;

	b  = r / BLOCK_B;
	r -= b * BLOCK_B;

	c  = r;
	
	assert (i == (a * BLOCK_A + b * BLOCK_B + c));

	pw[0] = wksq [a];
	pb[0] = bksq [a];

	x = aabase [b];
	y = (b + 1) + x - (x * (127-x)/2);

	pw[1] = x;
	pw[2] = y;
	pw[3] = c;
	pw[4] = NOSQUARE;

	pb[1] = NOSQUARE;
	
	assert (kaabk_pctoindex (pw, pb, &a) && a == i);

	return;
}

static bool_t 
kaabk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out)
{
	enum  {N_WHITE = 4, N_BLACK = 1};
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki, ai;
	unsigned int ft;
	int i;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */
	ai = aaidx [ws[1]] [ws[2]];

	if (ki == NOINDEX || ai == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ai * BLOCK_B + ws[3]; 
	return TRUE;
}

/****************** End KAAB/K *************************************/

/********************** KABB/K ************************************/

static bool_t 	test_kabbk (void);
static bool_t 	kabbk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kabbk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kabbk (void)
{

	enum  {MAXPC = 16+1};
	char 		str[] = "kabbk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kabbk_pctoindex (pw, pb, &i)) {
							kabbk_indextopc (i, px, py);		
							kabbk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kabbk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	index_t a, b, c, r, x, y;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;

	b  = r / BLOCK_B;
	r -= b * BLOCK_B;

	c  = r;
	
	assert (i == (a * BLOCK_A + b * BLOCK_B + c));

	pw[0] = wksq [a];
	pb[0] = bksq [a];

	x = aabase [b];
	y = (b + 1) + x - (x * (127-x)/2);

	pw[1] = c;
	pw[2] = x;
	pw[3] = y;
	pw[4] = NOSQUARE;

	pb[1] = NOSQUARE;
	
	assert (kabbk_pctoindex (pw, pb, &a) && a == i);

	return;
}

static bool_t 
kabbk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, /*@out@*/ index_t *out)
{
	enum  {N_WHITE = 4, N_BLACK = 1};
	enum  {
			BLOCK_B = 64,
			BLOCK_A = BLOCK_B * MAX_AAINDEX 
		}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki, ai;
	unsigned int ft;
	int i;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */
	ai = aaidx [ws[2]] [ws[3]];

	if (ki == NOINDEX || ai == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ai * BLOCK_B + ws[1]; 
	return TRUE;
}

/********************** End KABB/K *************************************/

/********************** init KAAA/K ************************************/

static index_t
aaa_getsubi (sq_t x, sq_t y, sq_t z);

static sq_t 		aaa_xyz [MAX_AAAINDEX] [3];
static index_t	 	aaa_base [64];

static index_t
init_aaa (void)
/* modifies aaa_base[], aaa_xyz[][] */
{
	index_t comb [64];
	index_t accum;
	index_t a;
	
	int index;
	SQUARE x, y, z;

	/* getting aaa_base */	
	comb [0] = 0;	
	for (a = 1; a < 64; a++) {
		comb [a] = a * (a-1) / 2;	
	}
	
	accum = 0;
	aaa_base [0] = accum;		
	for (a = 0; a < (64-1); a++) {
		accum += comb[a];
		aaa_base [a+1] = accum;	
	}

	assert ((accum + comb[63]) == MAX_AAAINDEX);
	/* end getting aaa_base */


	/* initialize aaa_xyz [][] */
	for (index = 0; index < MAX_AAAINDEX; index++) {
		aaa_xyz[index][0] = NOINDEX;
		aaa_xyz[index][1] = NOINDEX;				
		aaa_xyz[index][2] = NOINDEX;

	}

	index = 0;
	for (z = 0; z < 64; z++) {
		for (y = 0; y < z; y++) {
			for (x = 0; x < y; x++) {
			
				assert (index == aaa_getsubi (x, y, z));
	
				aaa_xyz [index] [0] = x;
				aaa_xyz [index] [1] = y;				
				aaa_xyz [index] [2] = z;
				
				index++;
			}	
		}
	}
	
	assert (index == MAX_AAAINDEX);

	return index;
}


static index_t
aaa_getsubi (sq_t x, sq_t y, sq_t z)
/* uses aaa_base */
{
	index_t calc_idx, base;
	
	assert (x < 64 && y < 64 && z < 64);
	assert (x < y && y < z);

	base = aaa_base[z];
	calc_idx = x + (y - 1) * y / 2 +  + base;

	return calc_idx;
}

/********************** KAAA/K ************************************/

static bool_t 	test_kaaak (void);
static bool_t 	kaaak_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kaaak_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kaaak (void)
{

	enum  {MAXPC = 16+1};
	char 		str[] = "kaaak";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kaaak_pctoindex (pw, pb, &i)) {
							kaaak_indextopc (i, px, py);		
							kaaak_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kaaak_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {
			BLOCK_A = MAX_AAAINDEX 
		};
	index_t a, b, r;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;

	b  = r;
	
	assert (i == (a * BLOCK_A + b));
	assert (b < BLOCK_A);

	pw[0] = wksq [a];
	pb[0] = bksq [a];

	pw[1] = aaa_xyz [b] [0];
	pw[2] = aaa_xyz [b] [1];
	pw[3] = aaa_xyz [b] [2];
	pw[4] = NOSQUARE;

	pb[1] = NOSQUARE;

	assert (kaaak_pctoindex (pw, pb, &a) && a == i);

	return;
}

static bool_t 
kaaak_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {N_WHITE = 4, N_BLACK = 1};
	enum  {
			BLOCK_A = MAX_AAAINDEX 
		}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki, ai;
	unsigned int ft;
	int i;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

	for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i]; ws[N_WHITE] = NOSQUARE;
	for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i]; bs[N_BLACK] = NOSQUARE;	

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}


	{ 
		SQUARE tmp;
		if (ws[2] < ws[1]) {
            tmp = ws[1];
            ws[1] = ws[2];
            ws[2] = tmp;
		}
		if (ws[3] < ws[2]) {
            tmp = ws[2];
            ws[2] = ws[3];
            ws[3] = tmp;
		}
		if (ws[2] < ws[1]) {
            tmp = ws[1];
            ws[1] = ws[2];
            ws[2] = tmp;
		}
	}
	
	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */

/*128 == (128 & (((ws[1]^ws[2])-1) | ((ws[1]^ws[3])-1) | ((ws[2]^ws[3])-1)) */
	
	if (ws[1] == ws[2] || ws[1] == ws[3] || ws[2] == ws[3]) {
		*out = NOINDEX;
		return FALSE;
	}
	
	ai = aaa_getsubi ( ws[1], ws[2], ws[3] );	
	
	if (ki == NOINDEX || ai == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ai; 
	return TRUE;
}

/****************** End KAAB/K *************************************/

/**********************  KAP/KB ************************************/

static bool_t 	test_kapkb (void);
static bool_t 	kapkb_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void	kapkb_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kapkb (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kapkb";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = d;
			pb[2] = NOSQUARE;
	
			if (kapkb_pctoindex (pw, pb, &i)) {
							kapkb_indextopc (i, px, py);		
							kapkb_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kapkb_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d * BLOCK_D + e; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 
	index_t a, b, c, d, e, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r / BLOCK_D;
	r -= d * BLOCK_D;
	e  = r;
	
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
	x ^= 070;        /* flip NS */

	pw[0] = b;
	pb[0] = c;	
	pw[1] = d;
	pw[2] = x;
	pw[3] = NOSQUARE;
	pb[1] = e;
	pb[2] = NOSQUARE;
	
	assert (kapkb_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kapkb_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 	
	SQUARE sq, pslice;
	SQUARE pawn = pw[2];
	SQUARE wa   = pw[1];
	SQUARE wk   = pw[0];
	SQUARE bk   = pb[0];
	SQUARE ba   = pb[1];
	
	assert (A2 <= pawn && pawn < A8);

	if (  !(A2 <= pawn && pawn < A8)) {
		*out = NOINDEX;
		return FALSE;
	}	
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
		ba   = flipWE (ba);		
	}

	sq = pawn;
	sq ^= 070; /* flipNS*/
	sq -= 8;   /* down one row*/
	pslice = (sq+(sq&3)) >> 1; 

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa * BLOCK_D + ba;

	return TRUE;
}
/********************** end KAP/KB ************************************/

/*************************  KAB/KP ************************************/

static bool_t 	test_kabkp (void);
static bool_t 	kabkp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void	kabkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kabkp (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kabkp";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (d <= H1 || d >= A8)
				continue;
			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = d;
			pb[2] = NOSQUARE;
	
			if (kabkp_pctoindex (pw, pb, &i)) {
							kabkp_indextopc (i, px, py);		
							kabkp_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kabkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d * BLOCK_D + e; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 
	index_t a, b, c, d, e, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r / BLOCK_D;
	r -= d * BLOCK_D;
	e  = r;
	
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
	/*x ^= 070;*/        /* do not flip NS */

	pw[0] = b;
	pb[0] = c;	
	pw[1] = d;
	pw[2] = e;
	pw[3] = NOSQUARE;
	pb[1] = x;
	pb[2] = NOSQUARE;
	
	assert (kabkp_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kabkp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 	
	SQUARE sq, pslice;
	SQUARE pawn = pb[1];
	SQUARE wa   = pw[1];
	SQUARE wk   = pw[0];
	SQUARE bk   = pb[0];
	SQUARE wb   = pw[2];
	
	assert (A2 <= pawn && pawn < A8);

	if (  !(A2 <= pawn && pawn < A8)) {
		*out = NOINDEX;
		return FALSE;
	}	
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
		wb   = flipWE (wb);		
	}

	sq = pawn;
	/*sq ^= 070;*/ /* do not flipNS*/
	sq -= 8;   /* down one row*/
	pslice = (sq+(sq&3)) >> 1; 

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa * BLOCK_D + wb;

	return TRUE;
}
/********************** end KAB/KP ************************************/




static void
kpk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	index_t a, b, c, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;
	
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
	x ^= 070;        /* flip NS */
	
	pw[1] = x;
	pw[0] = b;
	pb[0] = c;

	pw[2] = NOSQUARE;
	pb[1] = NOSQUARE;
	
	assert (kpk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kpk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 	
	SQUARE sq, pslice;
	SQUARE pawn = pw[1];
	SQUARE wk   = pw[0];
	SQUARE bk   = pb[0];

	#ifdef DEBUG
	if (  !(A2 <= pawn && pawn < A8)) {
		SQ_CONTENT wp[MAX_LISTSIZE], bp[MAX_LISTSIZE];
        bp [0] = wp[0] = KING;
        wp[1] = PAWN;
        wp[2] = bp[1] = NOPIECE;
		output_state (0, pw, pb, wp, bp);
	}	
	#endif

	assert (A2 <= pawn && pawn < A8);

	if (  !(A2 <= pawn && pawn < A8)) {
		*out = NOINDEX;
		return FALSE;
	}	
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
	}

	sq = pawn;
	sq ^= 070; /* flipNS*/
	sq -= 8;   /* down one row*/
	pslice = (sq+(sq&3)) >> 1; 

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk;

	return TRUE;
}



/**********************  KPP/K ************************************/

static bool_t 	test_kppk (void);
static bool_t 	kppk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kppk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kppk (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kppk";
	int 		a, b, c, d;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
			sq_t anchor1, anchor2, loosen1, loosen2;
			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;

			pp_putanchorfirst (b, c, &anchor1, &loosen1);
			pp_putanchorfirst (c, b, &anchor2, &loosen2);
			if (!(anchor1 == anchor2 && loosen1 == loosen2)) {
				printf ("Output depends on input in pp_outanchorfirst()\n input:%d, %d\n",b,c);
				fatal_error();
			} 
		}
	}


	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {


			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;

			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = d;
			pb[1] = NOSQUARE;
	
			if (kppk_pctoindex (pw, pb, &i)) {
							kppk_indextopc (i, px, py);		
							kppk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}

static void
kppk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	index_t a, b, c, r;
	int m, n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;

	m = pp_hi24 [a];
	n = pp_lo48 [a];
	
	pw[0] = b;
	pb[0] = c;	
	pb[1] = NOSQUARE;	
	
	pw[1] = pidx24_to_wsq (m);
	pw[2] = pidx48_to_wsq (n);

	pw[3] = NOSQUARE;


	assert (A2 <= pw[1] && pw[1] < A8);
	assert (A2 <= pw[2] && pw[2] < A8);

#ifdef DEBUG
	if (!(kppk_pctoindex (pw, pb, &a) && a == i)) {
		pc_t wp[] = {KING, PAWN, PAWN, NOPIECE};
		pc_t bp[] = {KING, NOPIECE};		
		printf("Indexes not matching: input:%d, output:%d\n", i, a);
		print_pos (pw, pb, wp, bp);
	}
#endif

	assert (kppk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kppk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 	
	int pp_slice;	
	SQUARE anchor, loosen;
	
	SQUARE wk     = pw[0];
	SQUARE pawn_a = pw[1];
	SQUARE pawn_b = pw[2];
	SQUARE bk     = pb[0];
	int i, j;

	#ifdef DEBUG
	if (!(A2 <= pawn_a && pawn_a < A8)) {
		printf ("\n\nsquare of pawn_a: %s\n", Square_str[pawn_a]);
		printf(" wk %s\n p1 %s\n p2 %s\n bk %s\n"
			, Square_str[wk]
			, Square_str[pawn_a]
			, Square_str[pawn_b]
			, Square_str[bk]
			);
	}
	#endif

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);

	pp_putanchorfirst (pawn_a, pawn_b, &anchor, &loosen);

	if ((anchor & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		anchor = flipWE (anchor);
		loosen = flipWE (loosen);
		wk     = flipWE (wk);		
		bk     = flipWE (bk);		
	}
 
	i = wsq_to_pidx24 (anchor);
	j = wsq_to_pidx48 (loosen);

	pp_slice = ppidx [i] [j];

	if (pp_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp_slice < MAX_PPINDEX );
	
	*out = pp_slice * BLOCK_A + wk * BLOCK_B  + bk;

	return TRUE;
}
/****************** end  KPP/K ************************************/

static void
kakp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
/*	x ^= 070;   */     /* flip NS */

	pw[0] = b;
	pb[0] = c;	
	pw[1] = d;
	pb[1] = x;
	pw[2] = NOSQUARE;
	pb[2] = NOSQUARE;
	
	assert (kakp_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kakp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 	
	SQUARE sq, pslice;
	SQUARE pawn = pb[1];
	SQUARE wa   = pw[1];
	SQUARE wk   = pw[0];
	SQUARE bk   = pb[0];

	assert (A2 <= pawn && pawn < A8);

	if (  !(A2 <= pawn && pawn < A8)) {
		*out = NOINDEX;
		return FALSE;
	}	
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
	}

	sq = pawn;
	/*sq ^= 070;*/ /* flipNS*/
	sq -= 8;   /* down one row*/
	pslice = (sq+(sq&3)) >> 1; 

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa;

	return TRUE;
}


static void
kapk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	/* x is pslice */
	x = a;
	x += x & B11100; /* get upper part and double it */
	x += 8;          /* add extra row  */
	x ^= 070;        /* flip NS */

	pw[0] = b;
	pb[0] = c;	
	pw[1] = d;
	pw[2] = x;
	pw[3] = NOSQUARE;
	pb[1] = NOSQUARE;
	
	assert (kapk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kapk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 	
	SQUARE sq, pslice;
	SQUARE pawn = pw[2];
	SQUARE wa   = pw[1];
	SQUARE wk   = pw[0];
	SQUARE bk   = pb[0];

	assert (A2 <= pawn && pawn < A8);

	if (  !(A2 <= pawn && pawn < A8)) {
		*out = NOINDEX;
		return FALSE;
	}	
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
	}

	sq = pawn;
	sq ^= 070; /* flipNS*/
	sq -= 8;   /* down one row*/
	pslice = (sq+(sq&3)) >> 1; 

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa;

	return TRUE;
}


static void
kaak_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	enum  {BLOCK_A = MAX_AAINDEX}; 
	index_t a, b, r, x, y;

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r;
	
	assert (i == (a * BLOCK_A + b));

	pw[0] = wksq [a];
	pb[0] = bksq [a];

	x = aabase [b];
	y = (b + 1) + x - (x * (127-x)/2);

	pw[1] = x;
	pw[2] = y;
	pw[3] = NOSQUARE;

	pb[1] = NOSQUARE;
	
	assert (kaak_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kaak_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out)
{
	enum  {N_WHITE = 3, N_BLACK = 1};
	enum  {BLOCK_A = MAX_AAINDEX}; 
	SQUARE ws[MAX_LISTSIZE], bs[MAX_LISTSIZE];
	index_t ki, ai;
	unsigned int ft;
	int i;

	ft = flipt [inp_pb[0]] [inp_pw[0]];

	assert (ft < 8);

    for (i = 0; i < N_WHITE; i++) ws[i] = inp_pw[i];
    ws[N_WHITE] = NOSQUARE;
    for (i = 0; i < N_BLACK; i++) bs[i] = inp_pb[i];
    bs[N_BLACK] = NOSQUARE;

	if ((ft & WE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipWE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipWE (bs[i]);
	}

	if ((ft & NS_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNS (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNS (bs[i]);
	}

	if ((ft & NW_SE_FLAG) != 0) {
		for (i = 0; i < N_WHITE; i++) ws[i] = flipNW_SE (ws[i]);
		for (i = 0; i < N_BLACK; i++) bs[i] = flipNW_SE (bs[i]);
	}

	ki = kkidx [bs[0]] [ws[0]]; /* kkidx [black king] [white king] */
	ai = aaidx [ws[1]] [ws[2]];

	if (ki == NOINDEX || ai == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	
	*out = ki * BLOCK_A + ai; 
	return TRUE;
}

/**********************  KPP/KA ************************************/

static bool_t 	test_kppka (void);
static bool_t 	kppka_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kppka_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kppka (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kppka";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;

			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = d;
			pb[2] = NOSQUARE;
	
			if (kppka_pctoindex (pw, pb, &i)) {
							kppka_indextopc (i, px, py);		
							kppka_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kppka_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/

	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;
	int m, n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	m = pp_hi24 [a];
	n = pp_lo48 [a];
	
	pw[0] = b;
	pw[1] = pidx24_to_wsq (m);
	pw[2] = pidx48_to_wsq (n);
	pw[3] = NOSQUARE;

	pb[0] = c;	
	pb[1] = d;
	pb[2] = NOSQUARE;	


	assert (A2 <= pw[1] && pw[1] < A8);
	assert (A2 <= pw[2] && pw[2] < A8);
	assert (kppka_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kppka_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	int pp_slice;	
	SQUARE anchor, loosen;
	
	SQUARE wk     = pw[0];
	SQUARE pawn_a = pw[1];
	SQUARE pawn_b = pw[2];
	SQUARE bk     = pb[0];
	SQUARE ba	  = pb[1];	
	int i, j;

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);

	pp_putanchorfirst (pawn_a, pawn_b, &anchor, &loosen);

	if ((anchor & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		anchor = flipWE (anchor);
		loosen = flipWE (loosen);
		wk     = flipWE (wk);		
		bk     = flipWE (bk);		
		ba	   = flipWE (ba);	
	}
 
	i = wsq_to_pidx24 (anchor);
	j = wsq_to_pidx48 (loosen);

	pp_slice = ppidx [i] [j];

	if (pp_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp_slice < MAX_PPINDEX );
	
	*out = pp_slice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + ba;

	return TRUE;
}

/********************** end KPP/KA ************************************/

/**********************  KAPP/K ************************************/

static bool_t 	test_kappk (void);
static bool_t 	kappk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kappk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kappk (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kappk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;

			
			pw[0] = a;
			pw[1] = d;	
			pw[2] = b;
			pw[3] = c;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kappk_pctoindex (pw, pb, &i)) {
							kappk_indextopc (i, px, py);		
							kappk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kappk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/

	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;
	int m, n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	m = pp_hi24 [a];
	n = pp_lo48 [a];
	
	pw[0] = b;
	pw[1] = d;
	pw[2] = pidx24_to_wsq (m);
	pw[3] = pidx48_to_wsq (n);
	pw[4] = NOSQUARE;

	pb[0] = c;	
	pb[1] = NOSQUARE;	


	assert (A2 <= pw[3] && pw[3] < A8);
	assert (A2 <= pw[2] && pw[2] < A8);
	assert (kappk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kappk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	int pp_slice;	
	SQUARE anchor, loosen;
	
	SQUARE wk     = pw[0];
	SQUARE wa	  = pw[1];	
	SQUARE pawn_a = pw[2];
	SQUARE pawn_b = pw[3];
	SQUARE bk     = pb[0];

	int i, j;

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);

	pp_putanchorfirst (pawn_a, pawn_b, &anchor, &loosen);

	if ((anchor & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		anchor = flipWE (anchor);
		loosen = flipWE (loosen);
		wk     = flipWE (wk);		
		bk     = flipWE (bk);		
		wa	   = flipWE (wa);	
	}
 
	i = wsq_to_pidx24 (anchor);
	j = wsq_to_pidx48 (loosen);

	pp_slice = ppidx [i] [j];

	if (pp_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp_slice < MAX_PPINDEX );
	
	*out = pp_slice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa;

	return TRUE;
}

/********************** end KAPP/K ************************************/

/**********************  KAPP/K ************************************/

static bool_t 	test_kapkp (void);
static bool_t 	kapkp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kapkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kapkp (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kapkp";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;

			
			pw[0] = a;
			pw[1] = d;	
			pw[2] = b;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = c;
			pb[2] = NOSQUARE;
	
			if (kapkp_pctoindex (pw, pb, &i)) {
							kapkp_indextopc (i, px, py);		
							kapkp_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static bool_t 
kapkp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 	
	int pp_slice;
	SQUARE anchor, loosen;
	
	SQUARE wk     = pw[0];
	SQUARE wa	  = pw[1];
	SQUARE pawn_a = pw[2];
	SQUARE bk     = pb[0];
	SQUARE pawn_b = pb[1];

	int m, n;

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);
	assert (pw[3] == NOSQUARE && pb[2] == NOSQUARE);

	anchor = pawn_a;
	loosen = pawn_b;

	if ((anchor & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		anchor = flipWE (anchor);
		loosen = flipWE (loosen);
		wk     = flipWE (wk);		
		bk     = flipWE (bk);		
		wa	   = flipWE (wa);		
	}
 
	m = wsq_to_pidx24 (anchor);
	n = loosen - 8;

	pp_slice = m * 48 + n; 

	if (pp_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp_slice < (64*MAX_PpINDEX) );
	
	*out = pp_slice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa;

	return TRUE;
}

static void
kapkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/
	enum  {BLOCK_A = 64*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	enum  {block_m = 48};
	index_t a, b, c, d, r;
	int m, n;
	SQUARE sq_m, sq_n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	/* unpack a, which is pslice, into m and n */
	r = a;
	m  = r / block_m;
	r -= m * block_m;
	n  = r ;

	sq_m = pidx24_to_wsq (m);
	sq_n = n + 8;
	
	pw[0] = b;
	pb[0] = c;	
	pw[1] = d;
	pw[2] = sq_m;
	pb[1] = sq_n;
	pw[3] = NOSQUARE;
	pb[2] = NOSQUARE;	
	
	assert (A2 <= sq_m && sq_m < A8);
	assert (A2 <= sq_n && sq_n < A8);
	assert (kapkp_pctoindex (pw, pb, &a) && a == i);

	return;
}

/********************** end KAP/KP ************************************/

/**********************  KABP/K ************************************/

static bool_t 	test_kabpk (void);
static bool_t 	kabpk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kabpk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kabpk (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kabpk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (d <= H1 || d >= A8)
				continue;
		
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kabpk_pctoindex (pw, pb, &i)) {
							kabpk_indextopc (i, px, py);		
							kabpk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kabpk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d * BLOCK_D + e; 
	*----------------------------------------------------------*/
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 
	index_t a, b, c, d, e, r;
	index_t x;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r / BLOCK_D;
	r -= d * BLOCK_D;
	e  = r;
	
	x = pidx24_to_wsq(a);

	pw[0] = b;
	pw[1] = d;
	pw[2] = e;
	pw[3] = x;
	pw[4] = NOSQUARE;

	pb[0] = c;	
	pb[1] = NOSQUARE;
	
	assert (kabpk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kabpk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64*64*64, BLOCK_B = 64*64*64, BLOCK_C = 64*64, BLOCK_D = 64}; 	
	SQUARE pslice;

	SQUARE wk   = pw[0];
	SQUARE wa   = pw[1];
	SQUARE wb   = pw[2];
	SQUARE pawn = pw[3];
	SQUARE bk   = pb[0];

	assert (A2 <= pawn && pawn < A8);
	
	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
		wb   = flipWE (wb);		
	}

	pslice = wsq_to_pidx24 (pawn);

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + wa * BLOCK_D + wb;

	return TRUE;
}

/********************** end KABP/K ************************************/

/**********************  KAAP/K ************************************/

static bool_t 	test_kaapk (void);
static bool_t 	kaapk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kaapk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kaapk (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kaapk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (d <= H1 || d >= A8)
				continue;
		
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kaapk_pctoindex (pw, pb, &i)) {
							kaapk_indextopc (i, px, py);		
							kaapk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kaapk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/
	enum 	{BLOCK_C = MAX_AAINDEX
			,BLOCK_B = 64*BLOCK_C
			,BLOCK_A = 64*BLOCK_B
	}; 	
	index_t a, b, c, d, r;
	index_t x, y, z;
	
	assert (i >= 0);

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;

	z = pidx24_to_wsq(a);
	
	/* split d into x, y*/
	x = aabase [d];
	y = (d + 1) + x - (x * (127-x)/2);

	assert (aaidx[x][y] == aaidx[y][x]);
	assert (aaidx[x][y] == d);


	pw[0] = b;
	pw[1] = x;
	pw[2] = y;
	pw[3] = z;
	pw[4] = NOSQUARE;
	
	pb[0] = c;	
	pb[1] = NOSQUARE;
	
	assert (kaapk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kaapk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum 	{BLOCK_C = MAX_AAINDEX
			,BLOCK_B = 64*BLOCK_C
			,BLOCK_A = 64*BLOCK_B
	}; 	
	index_t aa_combo, pslice;

	SQUARE wk   = pw[0];
	SQUARE wa   = pw[1];
	SQUARE wa2  = pw[2];
	SQUARE pawn = pw[3];
	SQUARE bk   = pb[0];

	assert (A2 <= pawn && pawn < A8);

	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
		wa2  = flipWE (wa2);
	}

	pslice = wsq_to_pidx24 (pawn);

	aa_combo = aaidx [wa] [wa2];

	if (aa_combo == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + aa_combo;

	assert (*out >= 0);

	return TRUE;
}

/********************** end KAAP/K ************************************/

/**********************  KAA/KP ************************************/

static bool_t 	test_kaakp (void);
static bool_t 	kaakp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kaakp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static bool_t
test_kaakp (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kaakp";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (d <= H1 || d >= A8)
				continue;
		
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = d;
			pb[2] = NOSQUARE;
	
			if (kaakp_pctoindex (pw, pb, &i)) {
							kaakp_indextopc (i, px, py);		
							kaakp_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kaakp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/
	enum 	{BLOCK_C = MAX_AAINDEX
			,BLOCK_B = 64*BLOCK_C
			,BLOCK_A = 64*BLOCK_B
	}; 	
	index_t a, b, c, d, r;
	index_t x, y, z;
	
	assert (i >= 0);

	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;

	z = pidx24_to_wsq(a); 
	z = flipNS(z);
	
	/* split d into x, y*/
	x = aabase [d];
	y = (d + 1) + x - (x * (127-x)/2);

	assert (aaidx[x][y] == aaidx[y][x]);
	assert (aaidx[x][y] == d);


	pw[0] = b;
	pw[1] = x;
	pw[2] = y;
	pw[3] = NOSQUARE;
	
	pb[0] = c;	
	pb[1] = z;
	pb[2] = NOSQUARE;
	
	assert (kaakp_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kaakp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum 	{BLOCK_C = MAX_AAINDEX
			,BLOCK_B = 64*BLOCK_C
			,BLOCK_A = 64*BLOCK_B
	}; 	
	index_t aa_combo, pslice;

	SQUARE wk   = pw[0];
	SQUARE wa   = pw[1];
	SQUARE wa2  = pw[2];
	SQUARE bk   = pb[0];
	SQUARE pawn = pb[1];

	assert (A2 <= pawn && pawn < A8);

	if ((pawn & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		pawn = flipWE (pawn);
		wk   = flipWE (wk);		
		bk   = flipWE (bk);		
		wa   = flipWE (wa);
		wa2  = flipWE (wa2);
	}

	pawn = flipNS(pawn);
	pslice = wsq_to_pidx24 (pawn);

	aa_combo = aaidx [wa] [wa2];

	if (aa_combo == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}	

	*out = pslice * BLOCK_A + wk * BLOCK_B  + bk * BLOCK_C + aa_combo;

	assert (*out >= 0);

	return TRUE;
}

/********************** end KAA/KP ************************************/

/**********************  KPP/KP ************************************/
/*
index_t 	pp48_idx[48][48];
sq_t		pp48_sq_x[MAX_PP48_INDEX];
sq_t		pp48_sq_y[MAX_PP48_INDEX]; 
*/
static bool_t 	test_kppkp (void);
static bool_t 	kppkp_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kppkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static int map24_b (sq_t s);
static sq_t unmap24_b (int i);

static index_t
init_pp48_idx (void)
/* modifies pp48_idx[][], pp48_sq_x[], pp48_sq_y[] */
{
	enum  {MAX_I = 48, MAX_J = 48};
	int i, j;
	int idx = 0;
	SQUARE a, b;

	/* default is noindex */
	for (i = 0; i < MAX_I; i++) {
		for (j = 0; j < MAX_J; j++) {
			pp48_idx [i][j] = NOINDEX;
		}
	}
		
	for (idx = 0; idx < MAX_PP48_INDEX; idx++) {
		pp48_sq_x [idx] = NOSQUARE;	
		pp48_sq_y [idx] = NOSQUARE;			
	}		
		
	idx = 0;
	for (a = H7; a >= A2; a--) {

		for (b = a - 1; b >= A2; b--) {

			i = flipWE( flipNS (a) ) - 8;
			j = flipWE( flipNS (b) ) - 8;
			
			if (pp48_idx [i] [j] == NOINDEX) {

				pp48_idx  [i][j]= idx; 	assert (idx < MAX_PP48_INDEX);
				pp48_idx  [j][i]= idx;
				pp48_sq_x [idx] = i; 	assert (i < MAX_I);
				pp48_sq_y [idx] = j; 	assert (j < MAX_J);		
				idx++;
			}
		}	
	}
	assert (idx == MAX_PP48_INDEX);
	return idx;
}



static bool_t
test_kppkp (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kppkp";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;
			if (d <= H1 || d >= A8)
				continue;
			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = d;
			pb[2] = NOSQUARE;
	
			if (kppkp_pctoindex (pw, pb, &i)) {
							kppkp_indextopc (i, px, py);		
							kppkp_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kppkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/

	enum  {BLOCK_A = MAX_PP48_INDEX*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	index_t a, b, c, d, r;
	int m, n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r / BLOCK_C;
	r -= c * BLOCK_C;	
	d  = r;
	
	m = pp48_sq_x [b];
	n = pp48_sq_y [b];
	
	pw[0] = c;
	pw[1] = flipWE(flipNS(m+8));
	pw[2] = flipWE(flipNS(n+8));
	pw[3] = NOSQUARE;

	pb[0] = d;	
	pb[1] = unmap24_b (a);
	pb[2] = NOSQUARE;	


	assert (A2 <= pw[1] && pw[1] < A8);
	assert (A2 <= pw[2] && pw[2] < A8);
	assert (A2 <= pb[1] && pb[1] < A8);
	assert (kppkp_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kppkp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = MAX_PP48_INDEX*64*64, BLOCK_B = 64*64, BLOCK_C = 64}; 
	int pp48_slice;	
	
	SQUARE wk     = pw[0];
	SQUARE pawn_a = pw[1];
	SQUARE pawn_b = pw[2];
	SQUARE bk     = pb[0];
	SQUARE pawn_c = pb[1];	
	int i, j, k;

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);
	assert (A2 <= pawn_c && pawn_c < A8);

	if ((pawn_c & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		wk     = flipWE (wk);		
		pawn_a = flipWE (pawn_a);
		pawn_b = flipWE (pawn_b);
		bk     = flipWE (bk);		
		pawn_c = flipWE (pawn_c);	
	}
 
	i = flipWE( flipNS (pawn_a) ) - 8;
	j = flipWE( flipNS (pawn_b) ) - 8;
	k = map24_b (pawn_c); /* black pawn, so low indexes mean more advanced 0 == A2 */

	pp48_slice = pp48_idx [i] [j];

	if (pp48_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp48_slice < MAX_PP48_INDEX );
	
	*out = k * BLOCK_A + pp48_slice * BLOCK_B + wk * BLOCK_C  + bk;

	return TRUE;
}

static int
map24_b (sq_t s)
{
	s -= 8;
	return ((s&3)+s)>>1;
}

static sq_t
unmap24_b (int i)
{
	return (i&(4+8+16)) + i + 8;
}

/********************** end KPP/KP ************************************/

/**********************  KPPP/K ************************************/

static const sq_t itosq[48] = {
	H7,G7,F7,E7,
	H6,G6,F6,E6,
	H5,G5,F5,E5,
	H4,G4,F4,E4,
	H3,G3,F3,E3,
	H2,G2,F2,E2,
	D7,C7,B7,A7,
	D6,C6,B6,A6,
	D5,C5,B5,A5,
	D4,C4,B4,A4,
	D3,C3,B3,A3,
	D2,C2,B2,A2
};

static bool_t 	test_kpppk (void);
static bool_t 	kpppk_pctoindex (const SQUARE *inp_pw, const SQUARE *inp_pb, index_t *out);
static void		kpppk_indextopc (index_t i, SQUARE *pw, SQUARE *pb);

static index_t
init_ppp48_idx (void)
/* modifies ppp48_idx[][], ppp48_sq_x[], ppp48_sq_y[], ppp48_sq_z[] */
{
	enum  {MAX_I = 48, MAX_J = 48, MAX_K = 48};
	int i, j, k;
	int idx = 0;
	SQUARE a, b, c;
	int x, y, z;

	/* default is noindex */
	for (i = 0; i < MAX_I; i++) {
		for (j = 0; j < MAX_J; j++) {
			for (k = 0; k < MAX_K; k++) {
				ppp48_idx [i][j][k] = (uint16_t)NOINDEX;
			}
		}
	}
		
	for (idx = 0; idx < MAX_PPP48_INDEX; idx++) {
		ppp48_sq_x [idx] = (uint8_t)NOSQUARE;	
		ppp48_sq_y [idx] = (uint8_t)NOSQUARE;	
		ppp48_sq_z [idx] = (uint8_t)NOSQUARE;		
	}		

	idx = 0;
	for (x = 0; x < 48; x++) {
		for (y = x+1; y < 48; y++) {
			for (z = y+1; z < 48; z++) {

				a = itosq [x];
				b = itosq [y];
				c = itosq [z];
		
				if (!in_queenside(b) || !in_queenside(c))			
						continue;

				i = a - 8;
				j = b - 8;
				k = c - 8;
				
				if (ppp48_idx [i] [j] [k] == (uint16_t)NOINDEX) {

					ppp48_idx  [i][j][k] = idx; 	
					ppp48_idx  [i][k][j] = idx;
					ppp48_idx  [j][i][k] = idx;
					ppp48_idx  [j][k][i] = idx;
					ppp48_idx  [k][i][j] = idx;
					ppp48_idx  [k][j][i] = idx;
					ppp48_sq_x [idx] = (uint8_t) i; 	assert (i < MAX_I);
					ppp48_sq_y [idx] = (uint8_t) j; 	assert (j < MAX_J);		
					ppp48_sq_z [idx] = (uint8_t) k; 	assert (k < MAX_K);	
					idx++;
				}
			}
		}	
	}

/*	assert (idx == MAX_PPP48_INDEX);*/
	return idx;
}

static bool_t
test_kpppk (void)
{

	enum 		{MAXPC = 16+1};
	char 		str[] = "kpppk";
	int 		a, b, c, d, e;
	SQUARE 		pw[MAXPC], pb[MAXPC];
	SQUARE 		px[MAXPC], py[MAXPC];	

	index_t		i, j;
	bool_t 		err = FALSE;

	printf ("%8s ", str);

	for (a = 0; a < 64; a++) {
		for (b = 0; b < 64; b++) {
		for (c = 0; c < 64; c++) {
		for (d = 0; d < 64; d++) {
		for (e = 0; e < 64; e++) {

			if (c <= H1 || c >= A8)
				continue;
			if (b <= H1 || b >= A8)
				continue;
			if (d <= H1 || d >= A8)
				continue;
			
			pw[0] = a;
			pw[1] = b;	
			pw[2] = c;
			pw[3] = d;
			pw[4] = NOSQUARE;
		
			pb[0] = e;
			pb[1] = NOSQUARE;
	
			if (kpppk_pctoindex (pw, pb, &i)) {
							kpppk_indextopc (i, px, py);		
							kpppk_pctoindex (px, py, &j);
							if (i != j) {
								err = TRUE;
							}
							assert (i == j);	
			}
	
		}
		}
		}
		}

        if ((a&1)==0) {
            printf(".");
            fflush(stdout);
        }
	}

	if (err)		
		printf ("> %s NOT passed\n", str);	
	else
		printf ("> %s PASSED\n", str);	
	return !err;
}


static void
kpppk_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c * BLOCK_C + d; 
	*----------------------------------------------------------*/

	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	index_t a, b, c, r;
	int m, n, o;
	
	r  = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;
	
	m = ppp48_sq_x [a];
	n = ppp48_sq_y [a];
	o = ppp48_sq_z [a];

	
	pw[0] = b;
	pw[1] = m + 8;
	pw[2] = n + 8;
	pw[3] = o + 8;
	pw[4] = NOSQUARE;

	pb[0] = c;	
	pb[1] = NOSQUARE;	


	assert (A2 <= pw[1] && pw[1] < A8);
	assert (A2 <= pw[2] && pw[2] < A8);
	assert (A2 <= pw[3] && pw[3] < A8);
	assert (kpppk_pctoindex (pw, pb, &a) && a == i);

	return;
}


static bool_t 
kpppk_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	int ppp48_slice;	
	
	SQUARE wk     = pw[0];
	SQUARE pawn_a = pw[1];
	SQUARE pawn_b = pw[2];
	SQUARE pawn_c = pw[3];	

	SQUARE bk     = pb[0];

	int i, j, k;

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);
	assert (A2 <= pawn_c && pawn_c < A8);

	i = pawn_a - 8;
	j = pawn_b - 8;
	k = pawn_c - 8;

	ppp48_slice = ppp48_idx [i] [j] [k];

	if (ppp48_slice == (uint16_t)NOINDEX) { 
		wk     = flipWE (wk);		
		pawn_a = flipWE (pawn_a);
		pawn_b = flipWE (pawn_b);
		pawn_c = flipWE (pawn_c);
		bk     = flipWE (bk);		
	}

	i = pawn_a - 8;
	j = pawn_b - 8;
	k = pawn_c - 8;

	ppp48_slice = ppp48_idx [i] [j] [k];
 
	if (ppp48_slice == (uint16_t)NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (ppp48_slice < MAX_PPP48_INDEX );
	
	*out = ppp48_slice * BLOCK_A + wk * BLOCK_B  + bk;

	return TRUE;
}


/********************** end KPPP/K ************************************/


static bool_t 
kpkp_pctoindex (const SQUARE *pw, const SQUARE *pb, index_t *out)
{
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 	
	int pp_slice;	
	SQUARE anchor, loosen;
	
	SQUARE wk     = pw[0];
	SQUARE bk     = pb[0];
	SQUARE pawn_a = pw[1];
	SQUARE pawn_b = pb[1];

	int m, n;

	#ifdef DEBUG
	if (!(A2 <= pawn_a && pawn_a < A8)) {
		printf ("\n\nsquare of pawn_a: %s\n", Square_str[pawn_a]);
		printf(" wk %s\n p1 %s\n p2 %s\n bk %s\n"
			, Square_str[wk]
			, Square_str[pawn_a]
			, Square_str[pawn_b]
			, Square_str[bk]
			);
	}
	#endif

	assert (A2 <= pawn_a && pawn_a < A8);
	assert (A2 <= pawn_b && pawn_b < A8);
	assert (pw[2] == NOSQUARE && pb[2] == NOSQUARE);

	/*pp_putanchorfirst (pawn_a, pawn_b, &anchor, &loosen);*/
	anchor = pawn_a;
	loosen = pawn_b;

	if ((anchor & 07) > 3) { /* column is more than 3. e.g. = e,f,g, or h */
		anchor = flipWE (anchor);
		loosen = flipWE (loosen);
		wk     = flipWE (wk);		
		bk     = flipWE (bk);		
	}
 
	m = wsq_to_pidx24 (anchor);
	n = loosen - 8;

	pp_slice = m * 48 + n; 

	if (pp_slice == NOINDEX) {
		*out = NOINDEX;
		return FALSE;
	}

	assert (pp_slice < MAX_PpINDEX );
	
	*out = pp_slice * BLOCK_A + wk * BLOCK_B  + bk;

	return TRUE;
}


static void
kpkp_indextopc (index_t i, SQUARE *pw, SQUARE *pb)
{
	/*---------------------------------------------------------*
		inverse work to make sure that the following is valid
		index = a * BLOCK_A + b * BLOCK_B + c; 
	*----------------------------------------------------------*/
	enum  {B11100  = 7u << 2};
	enum  {BLOCK_A = 64*64, BLOCK_B = 64}; 
	enum  {block_m = 48};
	index_t a, b, c, r;
	int m, n;
	SQUARE sq_m, sq_n;
	
	r = i;
	a  = r / BLOCK_A;
	r -= a * BLOCK_A;
	b  = r / BLOCK_B;
	r -= b * BLOCK_B;	
	c  = r;
	
	/* unpack a, which is pslice, into m and n */
	r = a;
	m  = r / block_m;
	r -= m * block_m;
	n  = r ;

	sq_m = pidx24_to_wsq (m);
	sq_n = n + 8;
	
	pw[0] = b;
	pb[0] = c;	
	pw[1] = sq_m;
	pb[1] = sq_n;
	pw[2] = NOSQUARE;
	pb[2] = NOSQUARE;	
	
	assert (A2 <= pw[1] && pw[1] < A8);
	assert (A2 <= pb[1] && pb[1] < A8);
/*	assert (kPpk_pctoindex (pw, pb, &a) && a == i);*/

	return;
}


/*--------------------------------------------------------------------------------*/

#ifdef BUILD_CODE

/*
|
|	Linearizing Bitboards 
|
\*---------------------------------------*/

static const int index64[64] = {
   63,  0, 58,  1, 59, 47, 53,  2,
   60, 39, 48, 27, 54, 33, 42,  3,
   61, 51, 37, 40, 49, 18, 28, 20,
   55, 30, 34, 11, 43, 14, 22,  4,
   62, 57, 46, 52, 38, 26, 32, 41,
   50, 36, 17, 19, 29, 10, 13, 21,
   56, 45, 25, 31, 35, 16,  9, 12,
   44, 24, 15,  8, 23,  7,  6,  5
};

#define debruijn64  U64(0x07EDD5E59A4E28C2)

static void
bitboard_to_sqlist (SQUARE *ps, BITBOARD a)
{
	uint64_t onebit;
	assert( ps != NULL);

	while (a != 0) {
		onebit = a & ~(a-1); /* a & -a negate */
		a &= ~onebit;
		*ps++ = index64[(onebit * debruijn64) >> 58];
	}
	*ps = NOSQUARE;
}

static void
sqlistfrom64 (sq_t *sql, uint64_t ib)
{
	BITBOARD bb = ib;
	bitboard_to_sqlist (sql, bb);
	return;
}

static int bitfromvector (uint64_t x);

static mv_t *
gen_pblockers (mv_t *pmove, 
					unsigned int stm, uint64_t p, uint64_t occ, uint64_t ib)
{
	static const sq_t promotion_options[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
	sq_t single_pawnstep [2] = { 8,  -8};
	sq_t double_pawnstep [2] = {16, -16};
	pc_t COLOR [2]           = {WHITES, BLACKS};
	uint64_t line2 [2]       = {U64(0x000000000000FF00) ,
                                U64(0x00FF000000000000)
                               };
	uint64_t pp, bb[2], pa, pb, x, first;
	sq_t from, to, step;
	unsigned promoted;
	unsigned color;
	mv_t mv;
	int z;
	/****/

	color = COLOR [stm];
	
	pp = p & line2[Opp(stm)]; /* promoting pawns */
	bb[0]  = pp << 8;
	bb[1]  = pp >> 8;
	pp = bb[stm];

	/* general */
	bb[0]  = p << 8;
	bb[1]  = p >> 8;
	pa = ib & bb[stm];
	
	pb = p & line2[stm];

	bb[0]  = pb << 8;
	bb[1]  = pb >> 8;
	pb = bb[stm];

	pb &= ~occ;
	
	bb[0]  = pb << 8;
	bb[1]  = pb >> 8;
	pb = bb[stm];

	pb &= ib;

	step = single_pawnstep [stm];

	x = pa & ~pp; /* non promoting pawn steps */
	
	while (x!= 0) {
		first = x & ~(x-1); /* x & -x; negate replaced by ~(x-1) */
		to = bitfromvector (first);
		from = to - step;
		MV_BUILD (mv, NORMAL_MOVE, color, PAWN, from, to, NOPIECE, NOPIECE);		
		*pmove++ = mv;
		x &= ~first;
	}

	x = pa & pp; /* promoting pawns steps */
	while (x!= 0) {
		first = x & ~(x-1); /* x & -x; negate replaced by ~(x-1) */
		to = bitfromvector (first);
		from = to - step;
		for (z = 0; z < 4; z++) {
			promoted = promotion_options[z]; 	
			MV_BUILD (mv, PROMOT_MOVE, color, PAWN, from, to, NOPIECE, promoted);		
			*pmove++ = mv;		
		}	
		x &= ~first;
	}

	step = double_pawnstep [stm];

	x = pb;
	while (x!= 0) {
		first = x & ~(x-1); /* x & -x; negate replaced by ~(x-1) */
		to = bitfromvector (first);
		from = to - step;
		MV_BUILD (mv, NORMAL_MOVE, color, PAWN, from, to, NOPIECE, NOPIECE);		
		*pmove++ = mv;
		x &= ~first;
	}
	*pmove = NOMOVE;
	return pmove;
}


static unsigned int bitlookup [16] = {
		 4,0,1,4,2,4,4,4,3,4,4,4,4,4,4,4
};


static int
bitfromvector (uint64_t x)
{
	uint32_t h;
	int offset, j;
	assert (0 == (x & (x-U64(1)))); /* only one bit */

	j = (((uint32_t) x - 1) >> 31)  <<  5;
	offset = j;
	h = (uint32_t)(x >> j);
	j = (((h & 0xFFFF) - 1) >> 31)  <<  4;
	offset += j;
	h >>= j;
	j = (((h & 0x00FF) - 1) >> 31)  <<  3;
	offset += j;
	h >>= j;	
	j = (((h & 0x000F) - 1) >> 31)  <<  2;
	offset += j;
	h >>= j;	

	return offset + bitlookup [h];
}

#endif



/****************************************************************************\
 *
 *
 *								DEBUG ZONE 
 *
 *
 ****************************************************************************/

#if defined(DEBUG) 
static void
print_pos (const sq_t *ws, const sq_t *bs, const pc_t *wp, const pc_t *bp)
{
	int i;
	printf ("White: ");
	for (i = 0; ws[i] != NOSQUARE; i++) {
		printf ("%s%s ", P_str[wp[i]], Square_str[ws[i]]);	
	}
	printf ("\nBlack: ");
	for (i = 0; bs[i] != NOSQUARE; i++) {
		printf ("%s%s ", P_str[bp[i]], Square_str[bs[i]]);	
	}
	printf ("\n");
}
#endif

#if defined(BUILD_CODE) || defined(DEBUG) || defined(FOLLOW_EGTB)
static void
output_state (unsigned int stm, const SQUARE *wSQ, const SQUARE *bSQ, 
								const SQ_CONTENT *wPC, const SQ_CONTENT *bPC)
{
	int i;
	assert (stm == WH || stm == BL);
	
	printf("\n%s to move\n", stm==WH?"White":"Black");
	printf("W: ");
	for (i = 0; wSQ[i] != NOSQUARE; i++) {
		printf("%s%s ", P_str[wPC[i]], Square_str[wSQ[i]]);
	} 				
	printf("\n");
	printf("B: ");	
	for (i = 0; bSQ[i] != NOSQUARE; i++) {
		printf("%s%s ", P_str[bPC[i]], Square_str[bSQ[i]]);
	} 
	printf("\n\n");
}
#endif

static void
list_index (void)
{
	enum  {START_GTB = 0, END_GTB = (MAX_EGKEYS)};
	int i; 
	unsigned long accum = 0;
	printf ("\nIndex for each GTB\n");
		printf ("%3s: %7s  %7s   %7s   %7s\n" , "i", "TB", "RAM-slice", "RAM-max", "HD-cumulative");	
	for (i = START_GTB; i < END_GTB; i++) { 
		unsigned long indiv_k  = egkey[i].maxindex * sizeof(dtm_t) * 2/1024;
		accum += indiv_k;
		printf ("%3d: %7s %8luk %8luk %8luM\n", i, egkey[i].str, indiv_k/egkey[i].slice_n, indiv_k, accum/1024/2);	
	}
	printf ("\n");	
	return;
}




