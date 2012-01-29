// $Id: filler.c -1   $

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

#include <time.h>
#include "common.h"
#include "filler.h"
#include "dicts.h"
#include "qxw.h"

static int ct_malloc=0,ct_free=0; // counters for debugging
static int phase=0; // state machine
static int winit=0; // count of words initialised so far
static int rcode=0; // return code: -3, -4: initflist errors; -2: out of stack; -1: out of memory; 1: no fill found; 2: fill found

// the following stacks keep track of the filler state as it recursively tries to fill the grid
#define MXSS (MXSZ*MXSZ+2)
static int sdep=-1; // stack pointer

static int sent[MXSS];             // entry considered at this depth
static char sposs[MXSS][MAXNL+1];  // possibilities for this entry, 0-terminated
static int spp[MXSS];              // which possibility we a currently trying (index into sposs)
static int*sflist[MXSS][NW];       // pointers to restore feasible word list flist
static int sflistlen[MXSS][NW];    // pointers to restore flistlen
static int scommit[MXSS][NW];      // word (if any - -1 otherwise) committed at this depth
static ABM sentryfl[MXSS][NE];     // feasible letter bitmap for this entry
//static int entryupd[NE];         // entry feasible letters have been updated
//static int wordupd[NW];          // word has been updated

#define isused(l) (lused[lts[l].uniq]|aused[lts[l].ans])
#define setused(l,v) lused[lts[l].uniq]=v,aused[lts[l].ans]=v

// phase 16: sort the feasible word list ready for display
static int sortwlist(void) {
// now done in mkfeas()
return 17;
}

// phase 17: transfer the feasible word list to the display
static int mkclist(void) {
updatefeas();
return 18;
}

// transfer progress info to display
static void progress(void) {
DEB1 printf("ct_malloc=%d ct_free=%d diff=%d\n",ct_malloc,ct_free,ct_malloc-ct_free);
updategrid();
}

// intersect light list q length l with letter position wp masked by bitmap m: result is stored in p and new length is returned
static int listisect(int*p,int*q,int l,int wp,ABM m) {int i,j;
for(i=0,j=0;i<l;i++) if(m&(1LL<<chartol[(int)(lts[q[i]].s[wp])])) p[j++]=q[i];
//printf("listisect l(wp=%d m=%16llx) %d->%d\n",wp,m,l,j);
return j;
}

// find the entry to expand next, or -1 if all done
static int findcritent(void) {int i,j,m;float k,l;
for(m=2;m>0;m--) { // m=2: loop over checked entries; then m=1: loop over unchecked entries
  j=-1;l=BIGF;
  for(i=0;i<ne;i++) {
    if(fillmode==2&&entries[i].sel==0) continue; // filling selection only: only check relevant entries
    if(entries[i].ch==' '&&entries[i].checking>=m) { // not already entered?
      k=entries[i].crux; // get the priority for this entry
      if(k<l) l=k,j=i;}
    }
  if(j!=-1) return j; // return entry with highest priority if one found
  }
return j; // return -1 if no entries left to expand
}

// check updated entries and rebuild feasible word lists
// returns -2 for infeasible, -1 for out of memory, 0 if no feasible word lists affected, >=1 otherwise
static int settleents(void) {struct entry*e;struct word*w;int f,i,l; int*p;
DEB1 printf("settleents() sdep=%d\n",sdep);
f=0;
for(i=0;i<nc;i++) cells[i].upd=0;
for(i=0;i<nc;i++) cells[i].upd|=cells[i].e->upd; // generate cell updated flags from entry updated flags
for(i=0;i<nc;i++) if(cells[i].upd) {
  e=cells[i].e;
  w=cells[i].w;
  p=w->flist;
  l=w->flistlen;
  if(sflistlen[sdep][w-words]==-1) {  // then we mustn't trash words[].flist
    sflist   [sdep][w-words]=w->flist;
    sflistlen[sdep][w-words]=w->flistlen;
    w->flist=(int*)malloc(l*sizeof(int)); // new list can be at most as long as old one
    if(w->flist==NULL) return -1; // out of memory
    ct_malloc++;
    }
  w->flistlen=listisect(w->flist,p,l,cells[i].wp,e->flbm); // generate new feasible word list
  if(w->flistlen!=l) {w->upd=1;f++;} // word list has changed: feasible letter lists will need updating
  if(w->flistlen==0&&!w->fe) return -2; // no options left, not a fully-entered word
  if(w->flistlen==1&&w->commit==-1) { // down to a single word?
    if(afunique&&isused(w->flist[0])) { // in no-duplicates mode and only answer left is already used? (could check all on list)
      w->flistlen=0; // abort
      return -2;
      }
    else { // otherwise, if down to one word, commit it
//      printf("committing word %d (%s)\n",w,lts[w->flist[0]].s);fflush(stdout);
      setused(w->flist[0],1); // flag as used
      w->commit=w->flist[0];
      scommit[sdep][w-words]=w->flist[0];
      }
    }
  }
for(i=0;i<ne;i++) entries[i].upd=0; // all entry update effects now propagated into word updates
// printf("settleents returns %d\n",f);fflush(stdout);
return f;
}

// check updated word lists, rebuild feasible entry lists
// returns 0 if no feasible letter lists affected, 1 otherwise
static int settlewds(void) {int ch,f,i,j,k,l,m; int*p;struct entry*e;
ABM cellfl[MXSZ];
DEB1 printf("settlewds()\n");
f=0;
for(i=0;i<nw;i++) if(words[i].upd) { // loop over updated word lists
  m=words[i].length;
  p=words[i].flist;
  l=words[i].flistlen;
  if(words[i].fe) continue;
  for(k=0;k<m;k++) {
    ch=words[i].c[k]->e->ch;
    if(ch!=' ') assert((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')),cellfl[k]=1LL<<chartol[ch]; // if already filled in, set single bit in bitmap
    else       cellfl[k]=0;
    }
  for(j=0;j<l;j++) for(k=0;k<m;k++) cellfl[k]|=1LL<<chartol[(int)lts[p[j]].s[k]]; // find all feasible letters from word list
  for(j=0;j<m;j++) {
    e=words[i].c[j]->e; // propagate from cell to entry
    if(e->flbm&~cellfl[j]) { // has this entry been changed by the additional constraint?
      e->flbm&=cellfl[j];
      e->upd=1;f++; // flag that it will need updating
//      printf("E%d %16llx\n",k,entries[k].flbm);fflush(stdout);
      }
    }
  }
for(i=0;i<nw;i++) words[i].upd=0; // all word list updates processed
// printf("settlewds returns %d\n",f);fflush(stdout);
return f;
}


// calculate per cell scores
static void mkscores(void) {int i,j,k,l,m; int*p; float f;
for(i=0;i<nc;i++) for(j=0;j<nl;j++) cells[i].score[j]=0.0;
for(i=0;i<nw;i++) {
  m=words[i].length;
  p=words[i].flist;
  l=words[i].flistlen;
//  if(p==NULL) {p=dwds[words[i].length];l=dcount[words[i].length];} // default feasible word list
  if(words[i].fe) continue;
  if(afunique&&words[i].commit>=0) {  // avoid zero score if we've committed
    if(l==1) for(k=0;k<m;k++) words[i].c[k]->score[chartol[(int)lts[p[0]].s[k]]]+=1.0;
    else     assert(l==0);
    }
  else {
    for(j=0;j<l;j++) if(!(afunique&&isused(p[j]))) { // for each remaining feasible word
      f=ansp[lts[p[j]].ans]->score;
      for(k=0;k<m;k++) words[i].c[k]->score[chartol[(int)lts[p[j]].s[k]]]+=f; // add in its score to this cell's score
      }
    }
  }
for(i=0;i<ne;i++) for(j=0;j<nl;j++) entries[i].score[j]=1.0;
for(i=0;i<nc;i++) {
//   f=(float)lcount[cells[i].w->length];
//  if(f!=0.0) f=1.0/f;
  f=1.0;
  for(j=0;j<nl;j++) cells[i].e->score[j]*=f*cells[i].score[j]; // copy scores to entries, scaled by total word count at this length
  }
for(i=0;i<ne;i++) {
  f=-BIGF; for(j=0;j<nl;j++) f=MX(f,entries[i].score[j]);
  entries[i].crux=f; // crux at an entry is the greatest score over all possible letters
  }
}


// sort possible letters into order of decreasing favour with randomness r; write results to s
void getposs(struct entry*e,char*s,int r) {int i,l,m,n;double j,k;
DEB2 printf("getposs(%d)\n",e-entries);
k=BIGF;l=0;
for(;;) {
  for(i=0,j=-BIGF;i<nl;i++) if(e->score[i]>j&&e->score[i]<k) j=e->score[i]; // peel off scores from top down
  DEB2 printf("getposs(%d): j=%g\n",e-entries,j);
  if(j<=0) break;
  for(i=0;i<nl;i++) if(e->score[i]==j) s[l++]=ltochar[i]; // add to output string
  k=j;} // get next highest set of equal scores
s[l]='\0';
if(r==0) return;
for(i=0;i<l;i++) { // randomise if necessary
  m=i+rand()%(r*2+1); // candidate for swap: distance depends on randomisation level
  if(m>=0&&m<l) n=s[i],s[i]=s[m],s[m]=n; // swap candidates
  }
}

// indent according to stack depth
static void sdepsp(void) {int i; if(sdep<0) printf("<%d",sdep); for(i=0;i<sdep;i++) printf(" ");}

// initialise state stacks
static void state_init(void) {sdep=-1;rcode=0;}

// push stack
static void state_push(void) {int i;
sdep++;
assert(sdep<MXSS);
for(i=0;i<nw;i++) sflistlen[sdep][i]=-1;  // flag that flists need allocating
for(i=0;i<nw;i++) scommit[sdep][i]=-1;    // flag no word committed at this depth
for(i=0;i<ne;i++) sentryfl[sdep][i]=entries[i].flbm; // feasible letter lists
}

// undo effect of last deepening operation
static void state_restore(void) {int i;
for(i=0;i<nw;i++) {
  if(scommit[sdep][i]!=-1) { // word to uncommit?
//    printf("uncommitting word %d (%s)\n",i,lts[scommit[sdep][i]].s);fflush(stdout);
    setused(scommit[sdep][i],0);
    words[i].commit=-1;
    scommit[sdep][i]=-1;
    }
  if(sflistlen[sdep][i]!=-1&&words[i].flist!=0) { // word feasible list to free?
    free(words[i].flist);
    ct_free++;
    words[i].flist=sflist[sdep][i];
    words[i].flistlen=sflistlen[sdep][i];
    }
  }
for(i=0;i<ne;i++) entries[i].flbm=sentryfl[sdep][i];
}

// pop stack
static void state_pop(void) {assert(sdep>=0);state_restore();sdep--;}

// clear state stacks and free allocated memory
static void state_finit(void) {while(sdep>=0) state_pop();}

// phase 10: initialise for search
static int searchinit(void) {int u,i,t0;
t0=clock();
for(i=winit;i<nw;i++) {
  FREEX(words[i].flist);
  lightx=words[i].gx0;
  lighty=words[i].gy0;
  lightdir=words[i].dir;
  u=getinitflist(&words[i].flist,&words[i].flistlen,words[i].lp,words[i].length);
  if(u) {rcode=-3;return 0;}
  if(words[i].lp->ten) clueorderindex++;
  if(clock()-t0>CLOCKS_PER_SEC/(20)) {
    DEB1 printf("Intercalated return while initialising word lists\n");
    winit=i+1;
    return 10;
    }
  }
if(postgetinitflist()) {rcode=-4;return 0;}
memset(aused,0,atotal);
memset(lused,0,ultotal);
return 14; // update internal data from initial grid
}

// phase 11: one level deeper in seach tree
static int searchdeeper(void) {int e; char s[MAXNL+1];
if(sdep==MXSS-1) {rcode=-2;return 15;}
DEB1 printf("mkscores...\n");
mkscores();
if(!fillmode) {rcode=2;return 15;}
DEB1 printf("findcritent...\n");
e=findcritent(); // find the most critical entry, over whose possible letters we will iterate
DEB1 printf("done\n");
if(e==-1) {rcode=2;return 15;} // all done, result found
getposs(entries+e,s,afrandom); // find feasible letter list in descending order of score
DEB1{sdepsp();printf("E%d %s\n",e,s);fflush(stdout);}
sent[sdep]=e;
spp[sdep]=0; // start on most likely possibility
strcpy(sposs[sdep],s);
return 12; // try this possibility
}

// phase 12: try one possibility at the current critical entry
static int searchtry(void) {char c; int e;
static clock_t ct0=0,ct1;
e=sent[sdep];
if(sposs[sdep][spp[sdep]]=='\0') {entries[e].ch=' ';return 13;} // none left: backtrack
c=sposs[sdep][spp[sdep]++]; // get letter to try
// sdepsp();printf(":%c:",c);fflush(stdout);
state_push();
entries[e].upd=1;entries[e].flbm=1LL<<chartol[(int)c]; // fix feasible list
entries[e].ch=c;
ct1=clock(); if(ct1-ct0>CLOCKS_PER_SEC*3||ct1-ct0<0) {progress();ct0=clock();} // update display every three seconds or so
return 14; // update internal data from new entry
}

// phase 13: backtrack when all possibilities in an entry have been exhausted without success
static int searchbacktrack(void) {
state_pop();
if(sdep==-1) {rcode=1;return 15;} // all done, no solution found
return 12; // try next possibility at level above
}

// phase 14: update internal data
static int searchsettle(void) {int f;
f=settleents(); // rescan entries
if(f==-2) return 13;
if(f==-1) {rcode=-1;return 15;} // out of memory: abort
f=settlewds(); // rescan words
if(f==0) return 11; // no changes: proceed to next search level
return 14; // need to iterate until everything settles down
}

// phase 15
static int searchdone(void) {int i;
progress(); // copy results to display
mkfeas(); // construct feasible word list
state_finit();
// if(fillmode&&currentdia) gtk_dialog_response(GTK_DIALOG(currentdia),GTK_RESPONSE_CANCEL);
for(i=0;i<nw;i++) assert(words[i].commit==-1); // ... and uncommitted
DEB1 printf("search done\n");
DEB1 printf(">> ct_malloc=%d ct_free=%d diff=%d\n",ct_malloc,ct_free,ct_malloc-ct_free);fflush(stdout);
// j=0; for(i=0;i<ltotal;i++) j+=isused[i]; printf("total lused=%d\n",j);fflush(stdout);
return 16;
}




// EXTERNAL INTERFACE

// state machine: each routine returns the next state
// return non-zero return code when nothing more to do or 0 if more work needed
int filler_step(void) {
DEB1 printf("(%d)\n",phase);fflush(stdout);
switch(phase) {
case  0:                        return rcode;
case 10:phase=searchinit();     return 0;
case 11:phase=searchdeeper();   return 0;
case 12:phase=searchtry();      return 0;
case 13:phase=searchbacktrack();return 0;
case 14:phase=searchsettle();   return 0;
case 15:phase=searchdone();     return 0;
case 16:phase=sortwlist();      return 0;
case 17:phase=mkclist();        return 0;
case 18:phase=0;                return 0;
default:assert(0);
  }
return 0;
}

// returns !=0 on error
int filler_start(void) {int i;
if(pregetinitflist()) return 1;
state_init();
for(i=0;i<nw;i++) words[i].commit=-1; // flag word uncommitted
state_push();
for(i=0;i<ne;i++) entries[i].upd=entries[i].flbm!=(1LL<<nl)-1;
for(i=0;i<nw;i++) words[i].upd=1;
winit=0;
clueorderindex=0;
phase=10;
return 0;
}
void filler_stop(void) {state_finit();}
