// $Id: common.h -1   $

/*
Qxw is a program to help construct and publish crosswords.

Copyright 2011 Mark Owen
http://www.quinapalus.com
E-mail: qxw@quinapalus.com

This file is part of Qxw.

Qxw is free software: you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.

Qxw is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Qxw.  If not, see <http://www.gnu.org/licenses/> or
write to the Free Software Foundation, Inc., 51 Franklin Street,
Fifth Floor, Boston, MA  02110-1301, USA.
*/


#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define RELEASE "20110826"

#define NGTYPE 5
#define MXSZ 63           // max grid size X and Y
#define MAXNDIR 3

/* big float that can be represented exactly: 2^100 */
#define BIGF (1267650600228229401496703205376.0)
#define PI 3.14159265359
#define ODD(x) ((x)&1)
#define EVEN(x) (!((x)&1))
#define FREEX(p) if(p) {free(p);p=0;}
#define SLEN 256 // maximum length for filenames, strings etc.

#define MX(a,b) ((a)>(b)?(a):(b))

extern int debug;
#define DEB1 if(debug&1)
#define DEB2 if(debug&2)
#define DEB4 if(debug&4)
#define DEB8 if(debug&8)

// Terminology:
// A `square' is the quantum of area in the grid; one or more squares (a `merge group') form
// an `entry', which is a single enclosed white area in the grid where a letter will appear.
// A `cell' corresponds to one letter of one of the `word's to be found to fill the grid.
// A checked grid square `entry' will
// have two or more corresponding cells, whose letters are constrained to agree.
// bldstructs() constructs the cells, words and entries from the square data produced
// by the editor.

#define NW (MXSZ*MXSZ*MAXNDIR)  // max words in grid
#define NC (MXSZ*MXSZ*MAXNDIR)  // max cells in grid
#define NE (MXSZ*MXSZ)    // max entries in grid
#define NS (MXSZ*MXSZ)    // max squares in grid
#define MAXNL 63          // maximum number of letters in alphabet

typedef unsigned long long int ABM; // alphabet bitmap type

struct sprop { // square properties
  unsigned int bgcol; // background colour
  unsigned int fgcol; // foreground colour
  unsigned char ten;  // treatment enable: unused at present
  unsigned char spor; // global square property override (not used in dsp)
  };

struct lprop { // light properties
  unsigned int dmask; // mask of allowed dictionaries
  unsigned int emask; // mask of allowed entry methods
  unsigned char ten;  // treatment enable
  unsigned char lpor; // global light property override (not used in dlp)
  };

struct cell {
  struct entry*e; // corresponding entry
  struct word*w; // corresponding word
  int wp; // position within word
  float score[MAXNL];
  char upd; // updated flag
  };
struct word {
  int length;
  int gx0,gy0; // start position (not necessarily mergerep)
  int dir;
  int*flist; // start of feasible list
  int flistlen; // length of feasible list
  int commit; // committed this word during fill?
  struct cell*c[MXSZ]; // list of cells making up this word
  struct lprop*lp; // applicable properties for this word
  char upd; // updated flag
  int fe; // fully-entered flag
  };
struct entry {
  ABM flbm; // feasible letter bitmap
  int gx,gy; // corresponding grid position (indices to gsq) of representative
  int checking; // count of corresponding cells
  float score[MAXNL];
  float crux; // priority
  char ch; // entered letter if any
  char sel; // selected flag
  char upd; // updated flag
  unsigned char fl; // flags copied from square
  };
struct square {
// source grid information (saved in undo buffers)
  unsigned int bars; // bar presence in each direction
  unsigned int merge; // merge flags in each direction
  unsigned char fl; // flags: b0=blocked, b3=not part of grid (for irregular shapes); b4=cell selected
  unsigned char dsel; // one light-selected flag for each direction
  char ch; // character entered or space
  struct sprop sp;
  struct lprop lp[MAXNDIR];

// derived information from here on
  struct entry*e;
  struct word*w[MAXNDIR];
  ABM flbm; // data provided by solver to running display (feasible letter bitmap)
  unsigned int vflags[MAXNDIR]; // violation flags: b0=double unch, b1=triple+ unch, b2=underchecked, b3=overchecked
  int number; // number in square (-1 for no number)
  int goi; // grid order index of treated squares (-1 in untreated squares)
  };
extern struct cell cells[NC];
extern struct word words[NW];
extern struct entry entries[NE];
extern struct square gsq[MXSZ][MXSZ];
extern int nw,nc,ne,ns;

// DICTIONARY

extern int chartol[256];
extern char ltochar[MAXNL];
extern int nl;

#define NLEM 2 // number of light entry methods
#define NATREAT 10 // number of treatments
#define NMSG 2 // number of messages passed to treatment code

struct answer { // a word found in one or more dictionaries
  int ahlink; // hash table link for isword()
  unsigned int dmask; // mask of dictionaries where word found
  unsigned int cfdmask; // mask of dictionaries where word found with this citation form
//  int light[NLEM]; // light indices of treated versions
  double score;
  char*cf; // citation form of the word in dstrings
  struct answer*acf; // alternative citation form (linked list)
  char*ul; // untreated light in dstrings: ansp is uniquified by this
  };

struct light { // a string that can appear in the grid, the result of treating an answer; not uniquified
  int hashslink; // hash table (s only) linked list
  int hashaeslink; // hash table (a,e,s) linked list
  int ans; // answer giving rise to this light
  int em; // mode of entry giving rise to this light
  char*s; // the light in dstrings, containing only chars in alphabet
  int uniq; // uniquifying number (by string s only), used as index into lused[]
  };

extern int*llist;               // buffer for word index list
extern int*llistp;              // ptr to matching word indices
extern int llistn;              // number of matching words
extern unsigned int llistdm;    // dictionary mask applicable to matching lights
extern int lcount[MXSZ+1];      // number of words of each length

extern struct answer**ansp;
extern struct light*lts;

extern int atotal;              // total answers in dict
extern int ultotal;             // total unique lights in dict
extern char*aused;              // answer already used
extern char*lused;              // light already used

extern char tpifname[SLEN];
extern int treatmode;
extern char treatmsg[NMSG][SLEN];
extern char treatmsgAZ[NMSG][SLEN];
extern int tambaw; // treated answer must be a word

extern int clueorderindex;
extern int lightx;
extern int lighty;
extern int lightdir;

// PREFERENCES

#define NPREFS 9
extern int prefdata[NPREFS];
#define clickblock (prefdata[0])
#define clickbar (prefdata[1])
#define shownums (prefdata[2])
#define mincheck (prefdata[3])
#define maxcheck (prefdata[4])
#define afunique (prefdata[5])
#define afrandom (prefdata[6])
#define eptsq (prefdata[7])
#define hpxsq (prefdata[8])

// FILLER

extern int fillmode; // 0=stopped, 1=filling all, 2=filling selection

#define UNDOS 100

#endif
