// $Id: qxw.c -1   $

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <wchar.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#include "common.h"
#include "qxw.h"
#include "filler.h"
#include "dicts.h"
#include "gui.h"
#include "draw.h"

// forward declarations
static void stepforw(int*x,int*y,int d);
static void stop_compute(void);

// GLOBAL PARAMETERS

// these are saved:

int width=12,height=12;          // grid size
int gtype=0;                     // grid type: 0=square, 1=hexH, 2=hexV, 3=circleA (edge at 12 o'clock), 4=circleB (cell at 12 o'clock)
int symmr=2,symmm=0,symmd=0;     // symmetry mode flags (rotation, mirror, up-and-down/left-and-right)
struct sprop dsp;                // default square properties
struct lprop dlp;                // default light properties

char gtitle[SLEN]="";
char gauthor[SLEN]="";

// these are not saved:
int debug=0;                     // debug level

char bname[SLEN]="untitled";     // name without file extensions
char filename[SLEN+50]="";           // name with file extension

int curx=0,cury=0,dir=0;         // cursor position, and direction: 0=to right, 1=down
int unsaved=0;                   // edited-since-save flag

int fillmode=0;                  // mode flag for filler: 0=stopped, 1=filling all, 2=filling selection

int ndir[NGTYPE]={2,3,3,2,2};                // number of directions per grid type
int dhflip[NGTYPE][MAXNDIR*2]={
{2,1,0,3},
{4,3,2,1,0,5},
{3,2,1,0,5,4},
{2,1,0,3},
{2,1,0,3}
};
int dvflip[NGTYPE][MAXNDIR*2]={
{0,3,2,1},
{1,0,5,4,3,2},
{0,5,4,3,2,1},
{2,1,0,3},
{2,1,0,3}
};



int*llistp=0;    // ptr to matching lights
int llistn=0;    // number of matching lights
unsigned int llistdm=0;   // dictionary mask applicable to matching lights
int*llist=0;     // buffer for light index list

// GRID

struct square gsq[MXSZ][MXSZ];

extern int getechar(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].ch;
  }

extern ABM getflbm(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].flbm;
  }

extern int getnumber(int x,int y) {
  return gsq[x][y].number;
  }

extern int getflags(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].fl;
  }

extern int getbgcol(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return (gsq[xr][yr].sp.spor?gsq[xr][yr].sp.bgcol:dsp.bgcol)&0xffffff;
  }

extern int getfgcol(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return (gsq[xr][yr].sp.spor?gsq[xr][yr].sp.fgcol:dsp.fgcol)&0xffffff;
  }

// move backwards one cell in direction d
void stepback(int*x,int*y,int d) {
  if(d>=ndir[gtype]) stepforw(x,y,d-ndir[gtype]);
  else switch(gtype) {
  case 0: if(d) (*y)--; else (*x)--;
    break;
  case 1: switch(d) {
    case 0: if(((*x)&1)==1) (*y)++; (*x)--; break;
    case 1: if(((*x)&1)==0) (*y)--; (*x)--; break;
    case 2: (*y)--;break;
      }
    break;
  case 2: switch(d) {
    case 2: if(((*y)&1)==1) (*x)++; (*y)--; break;
    case 1: if(((*y)&1)==0) (*x)--; (*y)--; break;
    case 0: (*x)--;break;
      }
    break;
  case 3:case 4: if(d) (*y)--; else *x=(*x+width-1)%width;
    break;
    }
  }

// move forwards one cell in direction d
static void stepforw(int*x,int*y,int d) {
  if(d>=ndir[gtype]) stepback(x,y,d-ndir[gtype]);
  else switch(gtype) {
  case 0: if(d) (*y)++; else (*x)++;
    break;
  case 1: switch(d) {
    case 0: (*x)++; if(((*x)&1)==1) (*y)--; break;
    case 1: (*x)++; if(((*x)&1)==0) (*y)++; break;
    case 2: (*y)++;break;
      }
    break;
  case 2: switch(d) {
    case 2: (*y)++; if(((*y)&1)==1) (*x)--; break;
    case 1: (*y)++; if(((*y)&1)==0) (*x)++; break;
    case 0: (*x)++;break;
      }
    break;
  case 3:case 4: if(d) (*y)++; else *x=(*x+1)%width;
    break;
    }
  }


int isingrid(int x,int y) {
  if(x<0||y<0||x>=width||y>=height) return 0;
  if(gtype==1&&ODD(width )&&ODD(x)&&y==height-1) return 0;
  if(gtype==2&&ODD(height)&&ODD(y)&&x==width -1) return 0;
  return 1;
  }

int isclear(int x,int y) {
  if(!isingrid(x,y)) return 0;
  if(gsq[x][y].fl&0x09) return 0;
  return 1;
  }

int isbar(int x,int y,int d) {int u;
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return 0;
  u=(gsq[x][y].bars>>d)&1;
  DEB2 printf("  x=%d y=%d d=%d u=%d g[x,y].bars=%08x\n",x,y,d,u,gsq[x][y].bars);
  stepforw(&x,&y,d);
  if(!isingrid(x,y)) return 0;
  return u;
  }

int ismerge(int x,int y,int d) {int u;
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return 0;
  u=(gsq[x][y].merge>>d)&1;
  stepforw(&x,&y,d);
  if(!isingrid(x,y)) return 0;
  return u;
  }

int sqexists(int i,int j) { // is a square within the grid (and not cut out)?
  if(!isingrid(i,j)) return 0;
  return !(gsq[i][j].fl&8);
  }

// is a step backwards clear? (not obstructed by a bar, block, cutout or edge of grid)
int clearbefore(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepback(&tx,&ty,d);
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  if(isbar(tx,ty,d)) return 0;
  return 1;
  }

// is a step forwards clear?
int clearafter(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepforw(&tx,&ty,d);
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  if(isbar(x,y,d)) return 0;
  return 1;
  }

// move forwards one (merged) cell in direction d
static void stepforwm(int*x,int*y,int d) {int x0,y0;
  x0=*x;y0=*y;
  while(ismerge(*x,*y,d)) {stepforw(x,y,d);if(*x==x0&&*y==y0) return;}
  stepforw(x,y,d);
  }

// is a step forwards clear?
static int clearafterm(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepforwm(&tx,&ty,d);
  if(x==tx&&y==ty) return 0;
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  stepback(&tx,&ty,d);
  if(isbar(tx,ty,d)) return 0;
  return 1;
  }

int stepbackifingrid (int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepback (&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;}
//static int stepforwifingrid (int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepforw (&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;} // not currently used
int stepforwmifingrid(int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepforwm(&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;}


void getmergerepd(int*mx,int*my,int x,int y,int d) {int x0,y0; // find merge representative, direction d (0<=d<ndir[gtype]) only
  assert(0<=d);
  assert(d<ndir[gtype]);
  *mx=x;*my=y;
  if(!isclear(x,y)) return;
  if(!ismerge(x,y,d+ndir[gtype])) return;
  x0=x;y0=y;
  do {
    stepback(&x,&y,d);
    if(!isclear(x,y)) goto ew1;
    if(x+y*MXSZ<*mx+*my*MXSZ) *mx=x,*my=y; // first lexicographically if loop
    if(x==x0&&y==y0) goto ew1; // loop detected
    } while(ismerge(x,y,d+ndir[gtype]));
  *mx=x;*my=y;
  ew1: ;
  }

int getmergedir(int x,int y) {int d; // get merge direction
  if(!isclear(x,y)) return -1;
  for(d=0;d<ndir[gtype];d++) if(ismerge(x,y,d)||ismerge(x,y,d+ndir[gtype])) return d;
  return 0;
  }

void getmergerep(int*mx,int*my,int x,int y) {int d; // find merge representative, any direction
  *mx=x;*my=y;
  d=getmergedir(x,y); if(d<0) return;
  getmergerepd(mx,my,x,y,d);
  }

int isownmergerep(x,y) {int x0,y0;
  getmergerep(&x0,&y0,x,y);
  return x==x0&&y==y0;
  }

int getmergegroupd(int*gx,int*gy,int x,int y,int d) {int l,x0,y0;
  if(!isclear(x,y)) {if(gx) *gx=x;if(gy) *gy=y;return 1;}
  getmergerepd(&x,&y,x,y,d);
  x0=x;y0=y;l=0;
  for(;;) {
    assert(l<MXSZ);
    if(gx) gx[l]=x;if(gy) gy[l]=y;l++;
    if(!ismerge(x,y,d)) break;
    stepforw(&x,&y,d);
    if(!isclear(x,y)) break;
    if(x==x0&&y==y0) break;
    }
  return l;
  }

int getmergegroup(int*gx,int*gy,int x,int y) {int d;
  if(!isclear(x,y)) {if(gx) *gx=x;if(gy) *gy=y;return 1;}
  d=getmergedir(x,y); assert(d>=0);
  return getmergegroupd(gx,gy,x,y,d);
  }

int isstartoflight(int x,int y,int d) {
  if(!isclear(x,y)) return 0;
  if(clearbefore(x,y,d)) return 0;
  if(clearafterm(x,y,d)) return 1;
  return 0;
  }

// is light selected?
int issellight(int x,int y,int d) {int l,lx,ly;
  l=getstartoflight(&lx,&ly,x,y,d);
  if(l<1) return 0;
  if(!isstartoflight(lx,ly,d)) return 0; // not actually a light
  return (gsq[lx][ly].dsel>>d)&1;
  }

// set selected flag on a light to k
void sellight(int x,int y,int d,int k) {int l,lx,ly;
  DEB1 printf("sellight(%d,%d,%d,%d)\n",x,y,d,k);
  l=getstartoflight(&lx,&ly,x,y,d);
  if(l<1) return;
  if(!isstartoflight(lx,ly,d)) return; // not actually a light
  gsq[lx][ly].dsel=(gsq[lx][ly].dsel&(~(1<<d)))|(k<<d);
  }

// returns:
// -1: looped, no light found
//  0: blocked, no light found
//  1: light found, start (not mergerep'ed) stored in *lx,*ly
int getstartoflight(int*lx,int*ly,int x,int y,int d) {int x0,y0;
  if(!isclear(x,y)) return 0;
  x0=x;y0=y;
  while(clearbefore(x,y,d)) {
    stepback(&x,&y,d); // to start of light
    if(x==x0&&y==y0) return -1; // loop found
    }
  *lx=x;*ly=y;
  return 1;
  }

// extract mergerepd'ed grid squares forming light running through (x,y) in direction d
// returns:
//   -1: loop detected
//    0: (x,y) blocked
//    1: no light in this direction (lx[0], ly[0] still set)
// n>=2: length of light (lx[0..n-1], ly[0..n-1] set)
int getlightd(int*lx,int*ly,int x,int y,int d) {int i;
  i=getstartoflight(&x,&y,x,y,d);
  if(i<1) return i;
  i=0;
  for(;;) {
    assert(i<MXSZ);
    lx[i]=x;ly[i]=y;i++;
    if(!clearafterm(x,y,d)) break;
    stepforwm(&x,&y,d);
    }
  return i;
  }

// extract all grid squares forming light running through (x,y) in direction d
// returns:
//   -1: loop detected
//    0: (x,y) blocked
//    1: no light in this direction (lx[0], ly[0] still set)
// n>=2: length of light (lx[0..n-1], ly[0..n-1] set)
int getlightda(int*lx,int*ly,int x,int y,int d) {int i;
  i=getstartoflight(&x,&y,x,y,d);
  if(i<1) return i;
  i=0;
  for(;;) {
    assert(i<MXSZ);
    lx[i]=x;ly[i]=y;i++;
    if(!clearafter(x,y,d)) break;
    stepforw(&x,&y,d);
    }
  return i;
  }

// extract merge-representative grid squares forming light running through (x,y) in direction d
int getlight(int*lx,int*ly,int x,int y,int d) {int i,l;
  l=getlightd(lx,ly,x,y,d);
  for(i=0;i<l;i++) getmergerep(lx+i,ly+i,lx[i],ly[i]);
  return l;
  }

// extract word running through x,y in direction d from grid
int getword(int x,int y,int d,char*s) {int i,l,lx[MXSZ],ly[MXSZ];
  l=getlight(lx,ly,x,y,d);
  for(i=0;i<l;i++) s[i]=gsq[lx[i]][ly[i]].ch;
  s[i]='\0';
  return l;
  }

// set character in mergrep square
void setch(int x,int y,char c) {
  getmergerep(&x,&y,x,y);
  gsq[x][y].ch=c;
  }






static void setmerge(int x,int y,int d,int k) { // set merge state in direction d to k, make bar state consistent
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return;
  if(k) k=1;
  gsq[x][y].merge&=~(1<<d);
  gsq[x][y].merge|=  k<<d;
  gsq[x][y].bars &=~gsq[x][y].merge;
  }

static void setbars(int x,int y,int d,int k) { // set bar state in direction d to k, make merge state consistent
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return;
  if(k) k=1;
  gsq[x][y].bars &=~(1<<d);
  gsq[x][y].bars |=  k<<d;
  gsq[x][y].merge&=~gsq[x][y].bars;
  }

static void demerge(int x,int y) {int i; // demerge from all neighbours
  for(i=0;i<ndir[gtype]*2;i++) setmerge(x,y,i,0);
  }

// calculate mask of feasible symmetries
int symmrmask(void) {int i,m;
  switch(gtype) {
case 0: return 0x16;break;
case 1:
    if(EVEN(width))          return 0x06;
    if(EVEN(width/2+height)) return 0x06;
    else                     return 0x5e;
    break;
case 2:
    if(EVEN(height))          return 0x06;
    if(EVEN(height/2+width))  return 0x06;
    else                      return 0x5e;
    break;
case 3: case 4:
    m=0;
    for(i=1;i<=12;i++) if(width%i==0) m|=1<<i;
    return m;
    break;
    }
  return 0x02;
  }

int symmmmask(void) {
  switch(gtype) {
case 0: case 1: case 2: return 0x0f;break;
case 3: case 4:
    if(width%2) return 0x03;
    else        return 0x0f;
    break;
    }
  return 0x01;
  }

int symmdmask(void) {
  switch(gtype) {
case 0: case 1: case 2:return 0x0f;break;
case 3: case 4:return 0x01;break;
    }
  return 0x01;
  }

int nw,nc,ne; // count of words, cells, entries
struct cell cells[NC];
struct word words[NW];
struct entry entries[NE];

// int gentry[MXSZ][MXSZ];  // convert grid coordinates to entry

// statistics calculated en passant by bldstructs()
int st_lc[MXSZ+1];    // total lights, by length
int st_lucc[MXSZ+1];  // underchecked lights, by length
int st_locc[MXSZ+1];  // overchecked lights, by length
int st_lsc[MXSZ+1];   // total of checked entries in lights, by length
int st_lmnc[MXSZ+1];  // minimum checked entries in lights, by length
int st_lmxc[MXSZ+1];  // maximum checked entries in lights, by length
int st_sc;            // total checked cells
int st_ce;            // total checked entries (i.e., squares)
int st_2u,st_3u;      // count of double+ and triple+ unches


int cbits(ABM x) {ABM i; int j; for(i=1,j=0;i<(1LL<<nl);i+=i) if(x&i) j++; return j;} // count set bits
int logbase2(ABM x) {ABM i; int j; for(i=1,j=0;i<(1LL<<nl);i+=i,j++) if(x&i) return j; return -1;} // find lowest set bit


// calculate numbers for squares and grid-order-indices of treated squares
void donumbers(void) {int d,i,i0,j,num=1,goi=0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) gsq[i][j].number=-1;
  for(j=0;j<height;j++) for(i0=0;i0<width;i0++) {
    if(gtype==1) {i=i0*2;if(i>=width) i=(i-width)|1;} // special case for hexH grid
    else i=i0;
    if(isclear(i,j)) {
      if(isownmergerep(i,j)&&(gsq[i][j].sp.spor?gsq[i][j].sp.ten:dsp.ten)) gsq[i][j].goi=goi++;
      else gsq[i][j].goi=-1;
      for(d=0;d<ndir[gtype];d++)
        if(isstartoflight(i,j,d)) {gsq[i][j].number=num++;break;} // start of a light?
      }
    }
  }

// build structures for solver and compile statistics
static int bldstructs(void) {int d,f,i,j,k,l,v,ti,tj,lx[MXSZ],ly[MXSZ]; unsigned long long int m; char c;
  nw=0; nc=0; ne=0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) {
    for(d=0;d<MAXNDIR;d++) gsq[i][j].w[d]=0,gsq[i][j].vflags[d]=0;
    gsq[i][j].e=0;
    gsq[i][j].flbm=0;
    gsq[i][j].number=-1;
    }
  for(i=0;i<MXSZ+1;i++) st_lc[i]=st_lucc[i]=st_locc[i]=st_lsc[i]=0,st_lmxc[i]=-1,st_lmnc[i]=99; // initialise stats
  for(j=0;j<height;j++) for(i=0;i<width;i++) if(isclear(i,j)&&isownmergerep(i,j)) {
      entries[ne].gx=i;entries[ne].gy=j;entries[ne].flbm=(1LL<<nl)-1;
      entries[ne].sel=!!(gsq[i][j].fl&16); // selected?
      gsq[i][j].e=entries+ne;
      ne++;} // find all entries (i.e., merge representatives)
    else gsq[i][j].e=0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) if(isclear(i,j)&&gsq[i][j].e==0) {
      getmergerep(&ti,&tj,i,j);
      gsq[i][j].e=gsq[ti][tj].e;
      }

  for(d=0;d<ndir[gtype];d++) {
    for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) { // look for lights
      words[nw].gx0=i;
      l=getlight(lx,ly,i,j,d);
      f=0;
      for(k=0;k<l;k++) if(gsq[lx[k]][ly[k]].ch==' ') f=1;
      words[nw].fe=!f; // fully entered?
      words[nw].gx0=i;
      words[nw].gy0=j;
      gsq[i][j].w[d]=words+nw;
      words[nw].dir=d;
      words[nw].lp=gsq[i][j].lp[d].lpor?&gsq[i][j].lp[d]:&dlp;
      for(k=0;k<l;k++) {
        DEB2 printf("d=%d x=%d y=%d k=%d\n",d,lx[k],ly[k],k);
        cells[nc].e=gsq[lx[k]][ly[k]].e;cells[nc].w=words+nw;cells[nc].wp=k;
        words[nw].c[k]=cells+nc;
        nc++;
        }
      words[nw].length=k;
      DEB2 printf("length=%d\n",k);
      nw++;
      }
    }
  for(i=0;i<ne;i++) {
    c=gsq[entries[i].gx][entries[i].gy].ch; entries[i].ch=c; // letter in square
    if(c==' ')  entries[i].flbm=(1LL<<nl)-1; // bitmap of feasible letters
    else        entries[i].flbm=1LL<<chartol[(int)c];
    }
  for(i=0;i<ne;i++) entries[i].checking=0; // now calculate checking (and other) statistics
  for(i=0;i<nc;i++) cells[i].e->checking++;
  st_ce=0;
  for(i=0;i<ne;i++) if(entries[i].checking>1) st_ce++; // number of checked squares
  st_sc=0;st_2u=0;st_3u=0;
  assert(MXSZ<64); // (we are about to use a bitmap of checking patterns)
  for(i=0;i<nw;i++) {
    l=words[i].length;
    st_lc[l]++;
    for(j=0,k=m=0;j<l;j++)
      if(words[i].c[j]->e->checking>1) k++; // count checked cells in word
      else m|=1<<j; // build up checking pattern bitmap
  // k is now number of checked cells (out of l total)
    v=0;
    if( k   *100<l*mincheck) st_lucc[l]++,v|=4; // violation flags
    if((k-1)*100>l*maxcheck) st_locc[l]++,v|=8;
    st_lsc[l]+=k;
    if(k<st_lmnc[l]) st_lmnc[l]=k;
    if(k>st_lmxc[l]) st_lmxc[l]=k;
    st_sc+=k;
    if(m&(m<<1)) st_2u++,v|=1; // double+ unch?
    if(m&(m<<1)&(m<<2)) st_3u++,v|=2; // triple+unch?
    for(j=0;j<l;j++) gsq[words[i].gx0][words[i].gy0].vflags[words[i].dir]|=v; // update violation flags
    }
  donumbers();
  stats_upd(); // refresh statistics window if it is in view
  // printf("nw=%d nc=%d ne=%d\n",nw,nc,ne);
  return 0;
  }

// symmetry functions
// call f on (x,y) if in range
static void symmdo5(void f(int,int,int,int),int k,int x,int y,int d) {
  if(isingrid(x,y)) f(k,x,y,d);
  }

// call symmdo5 on (x,y) and other square implied by up-and-down symmetry flag (if any)
static void symmdo4(void f(int,int,int,int),int k,int x,int y,int d) {
  if(symmd&2) switch(gtype) {
case 0: case 1: case 2:
    symmdo5(f,k,x,(y+(height+1)/2)%((height+1)&~1),d);break;
case 3: case 4: break; // not for circular grids
    }
  symmdo5(f,k,x,y,d);
  }

// call symmdo4 on (x,y) and other square implied by left-and-right symmetry flag (if any)
static void symmdo3(void f(int,int,int,int),int k,int x,int y,int d) {
  if(symmd&1) switch(gtype) { 
case 0: case 1: case 2:
    symmdo4(f,k,(x+(width+1)/2)%((width+1)&~1),y,d);break;
case 3: case 4: break; // not for circular grids
    }
  symmdo4(f,k,x,y,d);
  }

// call symmdo3 on (x,y) and other square implied by vertical mirror flag (if any)
static void symmdo2(void f(int,int,int,int),int k,int x,int y,int d) {int h;
  h=height;
  if(gtype==1&&ODD(width)&&ODD(x)) h--;
  if(symmm&2) switch(gtype) {
case 0: case 1: case 2:
    symmdo3(f,k,x,h-y-1,dvflip[gtype][d]);break;
case 3:
    if(width%2) break; // not for odd-size circular grids
    symmdo3(f,k,(width*3/2-x-1)%width,y,dvflip[gtype][d]);
    break;
case 4:
    if(width%2) break; // not for odd-size circular grids
    symmdo3(f,k,(width*3/2-x)%width,y,dvflip[gtype][d]);
    break;
    } 
  symmdo3(f,k,x,y,d);
  }

// call symmdo2 on (x,y) and other square implied by horizontal mirror flag (if any)
static void symmdo1(void f(int,int,int,int),int k,int x,int y,int d) {int w;
  w=width;
  if(gtype==2&&ODD(height)&&ODD(y)) w--;
  if(symmm&1) switch(gtype) {
case 0: case 1: case 2: case 3:
    symmdo2(f,k,w-x-1,y,dhflip[gtype][d]);break;
case 4:
    symmdo2(f,k,(w-x)%w,y,dhflip[gtype][d]);break;
    break;
    } 
  symmdo2(f,k,x,y,d);
  }

// get centre of rotation in 6x h-units, 4x v-units (gtype=1; mutatis mutandis for gtype=2)
static void getcrot(int*cx,int*cy) {int w,h;
  w=width;h=height;
  if(gtype==1) {
    *cx=(w-1)*3;
    if(EVEN(w)) *cy=h*2-1;
    else        *cy=h*2-2;
  } else {
    *cy=(h-1)*3;
    if(EVEN(h)) *cx=w*2-1;
    else        *cx=w*2-2;
    }
  }

// rotate by d/6 of a revolution in 6x h-units, 4x v-units (gtype=1; mutatis mutandis for gtype=2)
static void rot6(int*x0,int*y0,int x,int y,int d) {int u,v;
  u=1;v=1;
  switch(d) {
case 0:u= x*2    ;v=   2*y;break;
case 1:u= x  -3*y;v= x+  y;break;
case 2:u=-x  -3*y;v= x-  y;break;
case 3:u=-x*2    ;v=  -2*y;break;
case 4:u=-x  +3*y;v=-x-  y;break;
case 5:u= x  +3*y;v=-x+  y;break;
    }
  assert(EVEN(u));assert(EVEN(v));
  *x0=u/2;*y0=v/2;
  }

// call f on (x,y) and any other squares implied by symmetry flags by
// calling symmdo1 on (x,y) and any other squares implied by rotational symmetry flags
void symmdo(void f(int,int,int,int),int k,int x,int y,int d) {int i,mx,my,x0,y0;
  switch(gtype) {
case 0:
    switch(symmr) {
    case 4:symmdo1(f,k,width-y-1,x,         (d+1)%4);
           symmdo1(f,k,y,        height-x-1,(d+3)%4);
    case 2:symmdo1(f,k,width-x-1,height-y-1,(d+2)%4);
    case 1:symmdo1(f,k,x,        y,          d);
      }
    break;
case 1:
    getcrot(&mx,&my);
    y=y*4+(x&1)*2-my;x=x*6-mx;
    for(i=5;i>=0;i--) {
      if((i*symmr)%6==0) {
        rot6(&x0,&y0,x,y,i);
        x0+=mx;y0+=my;
        assert(x0%6==0); x0/=6;
        y0-=(x0&1)*2;
        assert(y0%4==0); y0/=4;
        symmdo1(f,k,x0,y0,(d+i)%6);
        }
      }
    break;
case 2:
    getcrot(&mx,&my);
    x=x*4+(y&1)*2-mx;y=y*6-my;
    for(i=5;i>=0;i--) {
      if((i*symmr)%6==0) {
        rot6(&y0,&x0,y,x,i);
        x0+=mx;y0+=my;
        assert(y0%6==0); y0/=6;
        x0-=(y0&1)*2;
        assert(x0%4==0); x0/=4;
        symmdo1(f,k,x0,y0,(d-i+6)%6);
        }
      }
    break;
case 3: case 4:
    if(symmr==0) break; // assertion
    for(i=symmr-1;i>=0;i--) symmdo1(f,k,(x+i*width/symmr)%width,y,d);
    break;
    }
  }

// basic grid editing commands (candidates for f() in symmdo above)
void a_editblock (int k,int x,int y,int d) {int l,gx[MXSZ],gy[MXSZ];
  l=getmergegroup(gx,gy,x,y);
  gsq[x][y].fl=(gsq[x][y].fl&0x06)|1;
  demerge(x,y);
  refreshsqlist(l,gx,gy);
  }

void a_editspace (int k,int x,int y,int d) {
  gsq[x][y].fl= gsq[x][y].fl&0x16;
  refreshsqmg(x,y);
  }

void a_editcutout(int k,int x,int y,int d) {int l,gx[MXSZ],gy[MXSZ];
  l=getmergegroup(gx,gy,x,y);
  gsq[x][y].fl=(gsq[x][y].fl&0x06)|8;
  demerge(x,y);
  refreshsqlist(l,gx,gy);
  }

// set bar state in direction d to k
void a_editbar(int k,int x,int y,int d) {int tx,ty;
  if(!isingrid(x,y)) return;
  tx=x;ty=y;
  stepforw(&tx,&ty,d);
  if(!isingrid(tx,ty)) return;
  setbars(x,y,d,k);
  refreshsqmg(x,y);
  refreshsqmg(tx,ty);
  donumbers();
  }

// set merge state in direction d to k, deal with consequences
void a_editmerge(int k,int x,int y,int d) {int f,i,l,tl,tx,ty,gx[MXSZ],gy[MXSZ],tgx[MXSZ],tgy[MXSZ];
  if(!isclear(x,y)) return;
  tx=x;ty=y;
  stepforw(&tx,&ty,d);
  if(!isclear(tx,ty)) return;
  l=getmergegroup(gx,gy,x,y);
  tl=getmergegroup(tgx,tgy,tx,ty);
  if(k) for(i=0;i<ndir[gtype];i++) if(i!=d&&i+ndir[gtype]!=d) {
    setmerge(x,y,i,0);
    setmerge(x,y,i+ndir[gtype],0);
    setmerge(tx,ty,i,0);
    setmerge(tx,ty,i+ndir[gtype],0);
    }
  setmerge(x,y,d,k);
  refreshsqlist(l,gx,gy);
  refreshsqlist(tl,tgx,tgy);
  f=gsq[x][y].fl&16; // make selection flags consistent
  l=getmergegroup(gx,gy,x,y);
  for(i=0;i<l;i++) gsq[gx[i]][gy[i]].fl=(gsq[gx[i]][gy[i]].fl&~16)|f;
  refreshsqmg(x,y);
  donumbers();
  }





// PREFERENCES

int prefdata[NPREFS]={0,0,0,66,75,1,0,36,36};

static char*prefname[NPREFS]={ // names as used in preferences file
  "edit_click_for_block", "edit_click_for_bar", "edit_show_numbers",
  "stats_min_check_percent", "stats_max_check_percent",
  "autofill_no_duplicates", "autofill_random",
  "export_EPS_square_points","export_HTML_square_pixels"};

static int prefminv[NPREFS]={0,0,0,0,0,0,0,10,10}; // legal range
static int prefmaxv[NPREFS]={1,1,1,100,100,1,2,72,72};

// read preferences from file
// fail silently
static void loadprefs(void) {FILE*fp;
  char s[SLEN],t[SLEN];
  int i,u;
  struct passwd*p;

  p=getpwuid(getuid());
  if(!p) return;
  if(strlen(p->pw_dir)>SLEN-20) return;
  strcpy(s,p->pw_dir);
  strcat(s,"/.qxw/preferences");
  DEB1 printf("loadprefs %s\n",s);
  fp=fopen(s,"r");if(!fp) return;
  while(fgets(s,SLEN,fp)) {
    if(sscanf(s,"%s %d",t,&u)==2) {
      for(i=0;i<NPREFS;i++)
        if(!strcmp(t,prefname[i])) {
          if(u<prefminv[i]) u=prefminv[i];
          if(u>prefmaxv[i]) u=prefmaxv[i];
          prefdata[i]=u;
          }
      }
    }
  fclose(fp);
  }

// write preferences to file
// fail silently
void saveprefs(void) {FILE*fp;
  int i;
  char s[SLEN];
  struct passwd*p;
  
  p=getpwuid(getuid());
  if(!p) return;
  if(strlen(p->pw_dir)>SLEN-20) return;
  strcpy(s,p->pw_dir);
  strcat(s,"/.qxw");
  mkdir(s,0777);
  strcat(s,"/preferences");
  DEB1 printf("saveprefs %s\n",s);
  fp=fopen(s,"w");if(!fp) return;
  for(i=0;i<NPREFS;i++) if(fprintf(fp,"%s %d\n",prefname[i],prefdata[i])<0) break;
  fclose(fp);
  }




// UNDO

static struct square ugsq[UNDOS][MXSZ][MXSZ]; // circular undo buffers
static int ugtype[UNDOS];
static int uwidth[UNDOS];
static int uheight[UNDOS];
static struct sprop udsp[UNDOS];
static struct lprop udlp[UNDOS];
int uhead=0,utail=0,uhwm=0; // undos can be performed from uhead back to utail, redos from uhead up to uhwm (`high water mark')

void undo_push(void) {int i,j;
  unsaved=1;
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) ugsq[uhead][i][j]=gsq[i][j]; // store state in undo buffer
  ugtype[uhead]=gtype;
  uwidth[uhead]=width;
  uheight[uhead]=height;
  udsp[uhead]=dsp;
  udlp[uhead]=dlp;
  uhead=(uhead+1)%UNDOS; // advance circular buffer pointer
  uhwm=uhead; // can no longer redo
  if(uhead==utail) utail=(utail+1)%UNDOS; // buffer full? delete one entry at tail
  DEB1 printf("undo_push: %d %d\n",uhead,utail);
  }

void undo_pop(void) {int i,j,u;
  uhead=(uhead+UNDOS-1)%UNDOS; // back head pointer up one
  u=(uhead+UNDOS-1)%UNDOS; // get previous state index
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) gsq[i][j]=ugsq[u][i][j]; // restore state
  gtype=ugtype[u];
  width=uwidth[u];
  height=uheight[u];
  dsp=udsp[uhead];
  dlp=udlp[uhead];
  }

void resetsp(struct sprop*p) {
  p->bgcol=0xffffff;
  p->fgcol=0x000000;
  p->ten=0;
  p->spor=0;
  }

void resetlp(struct lprop*p) {
  p->dmask=1;
  p->emask=1;
  p->ten=0;
  p->lpor=0;
  }

void resetstate(void) {int i,j,k;
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) {
    gsq[i][j].ch=' ';gsq[i][j].flbm=(1LL<<nl)-1;gsq[i][j].fl=0;gsq[i][j].bars=0;gsq[i][j].merge=0;gsq[i][j].dsel=0;
    resetsp(&gsq[i][j].sp);
    for(k=0;k<MAXNDIR;k++) resetlp(&gsq[i][j].lp[k]);
    }
  resetsp(&dsp);
  resetlp(&dlp);
  unsaved=0;
  bldstructs();
  }

// tidy up square properties structure
static void fixsp(struct sprop*sp) {
  sp->bgcol&=0xffffff;
  sp->fgcol&=0xffffff;
  sp->ten=!!sp->ten;
  sp->spor=!!sp->spor;
  }

// tidy up light properties structure
static void fixlp(struct lprop*lp) {
  lp->dmask&=(1<<MAXNDICTS)-1;
  lp->emask&=(1<<NLEM)-1;
  lp->ten=!!lp->ten;
  lp->lpor=!!lp->lpor;
  }

// FILE SAVE/LOAD

void a_filenew(void) {
  resetstate();
  setbname("untitled");
  syncgui();
  compute();
  undo_push();
  unsaved=0;
  }

#define NEXTL {if(!fgets(s,SLEN*4-1,fp)) goto ew3; l=strlen(s); while(l>0&&!isprint(s[l-1])) s[--l]='\0';}
// load state from file
void a_load(void) {
  int d,i,j,l,u,t0,t1,b,m,f; char *p,s[SLEN*4],*t,c;
  struct sprop sp;
  struct lprop lp;
  FILE*fp;

  fp=fopen(filename,"r");
  if(!fp) {fserror();return;}
  resetstate();
  *gtitle=0;
  *gauthor=0;
  setbname(filename);
  NEXTL;
  if(strncmp(s,"#QXW2",5)) {
  // LEGACY LOAD
    gtype=0;
    if(sscanf(s,"%d %d %d %d %d\n",&width,&height,&symmr,&symmm,&symmd)!=5) goto ew1;
    if(width<1||width>MXSZ|| // validate basic parameters
       height<1||height>MXSZ||
       symmr<0||symmr>2||
       symmm<0||symmm>3||
       symmd<0||symmd>3) goto ew1;
    if     (symmr==1) symmr=2;
    else if(symmr==2) symmr=4;
    else symmr=1;
    draw_init();
    for(j=0;j<height;j++) { // read flags
      NEXTL;
      t=s;
      for(i=0;i<width;i++) {
        u=strtol(t,&t,10);
        if(u<0||u>31) goto ew1;
        gsq[i][j].fl=u&0x19;
        gsq[i][j].bars=(u>>1)&3;
        gsq[i][j].merge=0;
        }
      }
    for(j=0;j<height;j++) { // read grid
      for(i=0;i<width;i++) {
        gsq[i][j].ch=fgetc(fp);
        if((gsq[i][j].ch<'A'||gsq[i][j].ch>'Z')&&(gsq[i][j].ch<'0'||gsq[i][j].ch>'9')&&gsq[i][j].ch!=' ') goto ew1;
        }
      if(fgetc(fp)!='\n') goto ew1;
      }
    }
  else {
  // #QXW2 load
    NEXTL;
    if(sscanf(s,"GP %d %d %d %d %d %d\n",&gtype,&width,&height,&symmr,&symmm,&symmd)!=6) goto ew1;
    if(gtype<0||gtype>=NGTYPE|| // validate basic parameters
       width<1||width>MXSZ||
       height<1||height>MXSZ) goto ew1;
    if(symmr<1||symmr>12) symmr=1;
    if(symmm<0||symmm>3) symmm=0;
    if(symmd<0||symmd>3) symmd=0;
    if((symmr&symmrmask())==0) symmr=1;
    draw_init();
    NEXTL;
    if(strcmp(s,"TTL")) goto ew1;
    NEXTL;
    if(s[0]!='+') goto ew1;
    strncpy(gtitle,s+1,SLEN-1);
    gtitle[SLEN-1]=0;
    NEXTL;
    if(strcmp(s,"AUT")) goto ew1;
    NEXTL;
    if(s[0]!='+') goto ew1;
    strncpy(gauthor,s+1,SLEN-1);
    gauthor[SLEN-1]=0;
    DEB1 printf("L0\n");
    NEXTL;
    if(!strncmp(s,"GLP ",4)) {
      if(sscanf(s,"GLP %d %d %hhd %hhd\n",&dlp.dmask,&dlp.emask,&dlp.ten,&dlp.lpor)!=4) goto ew1;
      dlp.lpor=0;
      fixlp(&dlp);
      NEXTL;
      }
    while(!strncmp(s,"GSP ",4)) {
      if(sscanf(s,"GSP %x %x %hhd %hhd\n",&dsp.bgcol,&dsp.fgcol,&dsp.ten,&dsp.spor)!=4) goto ew1;
      dsp.spor=0;
      fixsp(&sp);
      NEXTL;
      }
    while(!strncmp(s,"TM ",3)) {
      if(sscanf(s,"TM %d %d %d\n",&j,&t0,&t1)!=3) goto ew1;
      NEXTL;
      if(j==0&&s[0]=='+') {
        if(t0<0||t0>=NATREAT) continue;
        treatmode=t0,tambaw=!!t1;
        if(treatmode==NATREAT-1) {
          strncpy(tpifname,s+1,SLEN-1);
          tpifname[SLEN-1]=0;
          }
        NEXTL;
        }
      }
    while(!strncmp(s,"TMSG ",5)) {
      if(sscanf(s,"TMSG %d %d\n",&j,&t0)!=2) goto ew1;
      NEXTL;
      if(j==0&&s[0]=='+') {
        if(t0<0||t0>=NMSG) continue;
        strncpy(treatmsg[t0],s+1,SLEN-1);
        treatmsg[t0][SLEN-1]=0;
        NEXTL;
        }
      }
    while(!strncmp(s,"DFN ",4)) {
      if(sscanf(s,"DFN %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dfnames[j],s+1,SLEN-1);
        dfnames[j][SLEN-1]=0;
        NEXTL;
        }
      }
    while(!strncmp(s,"DSF ",4)) {
      if(sscanf(s,"DSF %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dsfilters[j],s+1,SLEN-1);
        dsfilters[j][SLEN-1]=0;
        NEXTL;
        }
      }
    while(!strncmp(s,"DAF ",4)) {
      if(sscanf(s,"DAF %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dafilters[j],s+1,SLEN-1);
        dafilters[j][SLEN-1]=0;
        NEXTL;
        }
      }
    DEB1 printf("L1: %s\n",s);

    while(!strncmp(s,"SQ ",3)) {
      c=' ';
      DEB1 printf("SQ: ");
      if(sscanf(s,"SQ %d %d %d %d %d %c\n",&i,&j,&b,&m,&f,&c)<5) goto ew1;
      DEB1 printf("%d,%d %d\n",i,j,b);
      if(i<0||i>=MXSZ||j<0||j>=MXSZ) continue;
      gsq[i][j].bars =b&((1<<ndir[gtype])-1);
      gsq[i][j].merge=m&((1<<ndir[gtype])-1);
      gsq[i][j].fl   =f&0x19;
      gsq[i][j].ch   =((c>='A'&&c<='Z')||(c>='0'&&c<='9'))?c:' ';
      NEXTL;
      }
    DEB1 printf("L2\n");
    while(!strncmp(s,"SQSP ",5)) {
      if(sscanf(s,"SQSP %d %d %x %x %hhd %hhd\n",&i,&j,&sp.bgcol,&sp.fgcol,&sp.ten,&sp.spor)!=6) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ) continue;
      fixsp(&sp);
      gsq[i][j].sp=sp;
      NEXTL;
      }
    while(!strncmp(s,"SQLP ",5)) {
      if(sscanf(s,"SQLP %d %d %d %d %d %hhd %hhd\n",&i,&j,&d,&lp.dmask,&lp.emask,&lp.ten,&lp.lpor)!=7) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ||d<0||d>=ndir[gtype]) continue;
      fixlp(&lp);
      gsq[i][j].lp[d]=lp;
      NEXTL;
      }
    }
  if(fclose(fp)) goto ew3;
  if(treatmode==NATREAT-1) {
    if((p=loadtpi())) {
      sprintf(s,"Error loading custom plug-in\n%.100s",p);
      reperr(s);
      }
    }
  else unloadtpi();
  donumbers();
  loaddicts(0);
  undo_push();unsaved=0;syncgui();compute();return; // no errors
  ew1:fclose(fp);syncgui();reperr("File format error");goto ew2;
  ew3:fserror();
  ew2:
  a_filenew();
  }

// write state to file
void a_save(void) {int d,i,j;
  FILE*fp;
  setbname(filename);
  fp=fopen(filename,"w");
  if(!fp) {fserror();return;}
  if(fprintf(fp,"#QXW2 http://www.quinapalus.com\n")<0) goto ew0;
  if(fprintf(fp,"GP %d %d %d %d %d %d\n",gtype,width,height,symmr,symmm,symmd)<0) goto ew0;
  if(fprintf(fp,"TTL\n+%s\n",gtitle)<0) goto ew0;
  if(fprintf(fp,"AUT\n+%s\n",gauthor)<0) goto ew0;
  if(fprintf(fp,"GLP %d %d %d %d\n",dlp.dmask,dlp.emask,dlp.ten,dlp.lpor)<0) goto ew0;
  if(fprintf(fp,"GSP %06x %06x %d %d\n",dsp.bgcol,dsp.fgcol,dsp.ten,dsp.spor)<0) goto ew0;
  if(fprintf(fp,"TM 0 %d %d\n+%s\n",treatmode,tambaw,tpifname)<0) goto ew0;
  for(i=0;i<NMSG;i++) if(fprintf(fp,"TMSG 0 %d\n+%s\n",i,treatmsg[i])<0) goto ew0;
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DFN %d\n+%s\n",i,dfnames[i])<0) goto ew0;
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DSF %d\n+%s\n",i,dsfilters[i])<0) goto ew0;
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DAF %d\n+%s\n",i,dafilters[i])<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) if(fprintf(fp,"SQ %d %d %d %d %d %c\n",i,j,gsq[i][j].bars,gsq[i][j].merge,gsq[i][j].fl,gsq[i][j].ch)<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++)
    if(fprintf(fp,"SQSP %d %d %06x %06x %d %d\n",i,j,gsq[i][j].sp.bgcol,gsq[i][j].sp.fgcol,gsq[i][j].sp.ten,gsq[i][j].sp.spor)<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) for(d=0;d<ndir[gtype];d++)
    if(fprintf(fp,"SQLP %d %d %d %d %d %d %d\n",i,j,d,gsq[i][j].lp[d].dmask,gsq[i][j].lp[d].emask,gsq[i][j].lp[d].ten,gsq[i][j].lp[d].lpor)<0) goto ew0;
  if(fprintf(fp,"END\n")<0) goto ew0;
  if(ferror(fp)) goto ew0;
  if(fclose(fp)) goto ew0;
  unsaved=0;return; // saved successfully
  ew0:
  fserror();
  fclose(fp);
  }

char*titlebyauthor(void) {static char t[SLEN*2+100];
  if(gtitle[0]) strcpy(t,gtitle);
  else          strcpy(t,bname);
  if(gauthor[0]) strcat(t," by "),strcat(t,gauthor);
  return t;
  }

// MAIN

#define NDEFDICTS 4
char*defdictfn[NDEFDICTS]={
"/usr/dict/words",
"/usr/share/dict/words",
"/usr/share/dict/british-english",
"/usr/share/dict/american-english"};

char*optarg;
int optind,opterr,optopt;

int main(int argc,char*argv[]) {int i,nd;

  for(i=0;i<26;i++) ltochar[i]=i+'A',   chartol[i+'A']=i;
  for(i=0;i<10;i++) ltochar[i+26]=i+'0',chartol[i+'0']=i+26;
  resetstate();
  for(i=0;i<MAXNDICTS;i++) dfnames[i][0]='\0';
  for(i=0;i<MAXNDICTS;i++) strcpy(dsfilters[i],"");
  for(i=0;i<MAXNDICTS;i++) strcpy(dafilters[i],"");
  freedicts();
  for(i=0;i<NW;i++) words[i].flist=0; 

  nd=0;
  i=0;
  for(;;) switch(getopt(argc,argv,"d:?D:")) {
  case -1: goto ew0;
  case 'd':
    if(strlen(optarg)<SLEN&&nd<MAXNDICTS) strcpy(dfnames[nd++],optarg);
    break;
  case 'D':debug=atoi(optarg);break;
  case '?':
  default:i=1;break;
    }

  ew0:
  if(i) {
  printf("Usage: %s [-d <dictionary_file>]* [qxw_file]\n",argv[0]);
  printf("This is Qxw, release %s.\n\n\
     Copyright 2011 Mark Owen\n\
     \n\
     This program is free software; you can redistribute it and/or modify\n\
     it under the terms of version 2 of the GNU General Public License as\n\
     published by the Free Software Foundation.\n\
     \n\
     This program is distributed in the hope that it will be useful,\n\
     but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
     GNU General Public License for more details.\n\
     \n\
     You should have received a copy of the GNU General Public License along\n\
     with this program; if not, write to the Free Software Foundation, Inc.,\n\
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n\
     \n\
     For more information visit http://www.quinapalus.com or\n\
     e-mail qxw@quinapalus.com\n\n\
",RELEASE);
    return 0;
    }

  gtk_init(&argc,&argv);
  startgtk();

  a_filenew(); // reset grid
  loadprefs(); // load preferences file (silently failing to defaults)
  if(optind<argc&&strlen(argv[optind])<SLEN) {
    strcpy(filename,argv[optind]);
    a_load();
  } else {
    if(nd) loaddicts(0);
    else { // no dictionary file specified, so run through some likely candidates
      for(i=0;i<NDEFDICTS;i++) {
        strcpy(dfnames[0],defdictfn[i]);
        strcpy(dsfilters[0],"^.*+(?<!'s)");
        loaddicts(1);
        if(atotal!=0) {nd++;break;} // happy if we found any words at all
        }
      if(nd==0) strcpy(dfnames[0],""),strcpy(dsfilters[0],"");
      }
    if(nd==0) reperr("No dictionaries loaded");
    }
  draw_init();
  syncgui();
  compute();
  gtk_main(); // main event loop
  stop_compute();
  draw_finit();
  stopgtk();
  freedicts();
  return 0;
  }




// INTERFACE TO FILLER STATE MACHINE

static int idletag,idletagged=0;

// called back by GTK when there are no events pending
static gboolean idle(gpointer data) {int i;
  DEB4 printf("I"),fflush(stdout);
  i=filler_step(); // run the filler for a while
  DEB1 printf("[%d]",i);
  if(i) { // finished?
    DEB4 printf("Fill finished (%d)",i),fflush(stdout);
    idletagged=0;
    if(fillmode) {
      killcurrdia();
      switch(i) {
      case -4:
      case -3: reperr("Error generating lists of feasible lights");break;
      case -2: reperr("Out of stack space");break;
      case -1: reperr("Out of memory");break;
      case 1: reperr("No fill found");compute();break;
        }
    } else {
      if(i!=2) { // background fill failed
        for(i=0;i<ne;i++) gsq[entries[i].gx][entries[i].gy].flbm=0; // clear feasible letter bitmaps
        llistp=NULL;llistn=0; // no feasible word list
        refreshhin();
        updatefeas();
        DEB4 printf("BG fill failed"),fflush(stdout);
        }
      }
    }
  return (gboolean)!i;
  }

// (re-)start the filler when an edit has been made
void compute(void) {
  filler_stop(); // stop if already running
  if(bldstructs()) return; // failed?
  if(filler_start()) {
    return;
    }
  if(idletagged) gtk_idle_remove(idletag);
  idletag=gtk_idle_add(idle,0); // add as an idle-time callback
  idletagged=1;
  setposslabel(" Working...");
  }

static void stop_compute(void) {
  filler_stop();
  }

// comparison function for sorting feasible word list by score
static int cmpscores(const void*p,const void*q) {float f,g;
  f=ansp[lts[*(int*)p].ans]->score;
  g=ansp[lts[*(int*)q].ans]->score;
  if(f<g) return  1;
  if(f>g) return -1;
  return 0;
  }

// called by filler when a list of feasible words through the cursor has been found
void mkfeas(void) {int l,x,y; int*p; struct word*w;
  llistp=NULL;llistn=0; // default answer: no list
  if(getstartoflight(&x,&y,curx,cury,dir)>0) w=gsq[x][y].w[dir];
  else w=0;
  DEB2 printf("mkfeas: %d,%d (d=%d) ->%d,%d, w=%p\n",curx,cury,dir,x,y,w);
  if(w==0||w->flist==0) {llistp=NULL;llistn=0;return;} // no list
  p=w->flist;
  l=w->flistlen;
  if(llist) free(llist);
  llist=(int*)malloc(l*sizeof(int)); // space for feasible word list
  if(llist==NULL) return;
  memcpy(llist,p,l*sizeof(int));llistp=llist;llistn=l;llistdm=w->lp->dmask; // copy list across
  qsort(llistp,llistn,sizeof(int),&cmpscores);
  DEB1 printf("mkfeas: %d matches; dm=%08x\n",llistn,llistdm);
  }

// provide progress info to display
void updategrid(void) {int i;
  for(i=0;i<ne;i++) gsq[entries[i].gx][entries[i].gy].flbm=entries[i].flbm; // copy feasible letter bitmaps
  refreshhin();
  }
