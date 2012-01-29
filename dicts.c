// $Id: dicts.c -1   $

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


// DICTIONARY

#include <wchar.h>
#include <pcre.h>
#include <dlfcn.h>
#include "common.h"
#include "gui.h"
#include "dicts.h"

// ISO-8859-1 character mapping to remove accents and fold case: #=reject word, .=ignore character for answer/light purposes
static char chmap[256]="\
.########..##.##\
################\
..#####.####....\
0123456789..###.\
#ABCDEFGHIJKLMNO\
PQRSTUVWXYZ####.\
#ABCDEFGHIJKLMNO\
PQRSTUVWXYZ#####\
################\
################\
.############.##\
####.###########\
AAAAAAECEEEEIIII\
#NOOOOO#OUUUUY##\
AAAAAAECEEEEIIII\
#NOOOOO#OUUUUY#Y";

int chartol[256];
char ltochar[MAXNL];
int nl=36; // needs to be initialised before call to resetstate()


#define MEMBLK 100000     // unit of memory allocation

struct memblk {
  struct memblk*next;
  int ct;
  char s[MEMBLK];
  };

static struct memblk*dstrings=0; // dictionary string pool

struct answer*ans=0,**ansp=0;
struct light*lts=0;

// int lcount[MXSZ+1]={0};      // number of lights of each length
int atotal=0;                // total answers
int ltotal=0;                // total lights
int ultotal=0;               // total uniquified lights
char*aused=0;                // answer already used while filling
char*lused=0;                // light already used while filling

char dfnames[MAXNDICTS][SLEN];
char dsfilters[MAXNDICTS][SLEN];
char dafilters[MAXNDICTS][SLEN];
char*lemdesc[NLEM]={"","(rev.)"};
char*lemdescADVP[NLEM]={"normally","backwards"};

#define HTABSZ 1048576
static int ahtab[HTABSZ];

// answer pool is linked list of `struct memblk's containing
// strings:
//   char 0 of string is word score*10+128 (in range 28..228);
//   char 1 of string is dictionary number
//   char 2 onwards:
//     (0-terminated) citation form, in UTF-8
//     (0-terminated) untreated light form, in chars

int loaddicts(int sil) { // load (or reload) dictionaries from dfnames[]
  // sil=1 suppresses error reporting
  // returns: 0=success; 4=no success (out of memory)
  FILE *fp;
  char s0[SLEN*2]; // citation form
  wchar_t ws[SLEN]; // citation form
  const wchar_t*wp;
  char s1[SLEN]; // light form
  char t[SLEN]; // input buffer
  const char*u;
  struct memblk*mp,*p,*q;
  struct answer*ap;
  int c,dn,i,j,k,l,l0,l1,m,ml,mode,r;
  unsigned int h;
  int f0,f1,f2,f3;
  //int lc[MXSZ+1];
  float f;
  mbstate_t ps;
  pcre*sre,*are;
  const char*pcreerr;
  int pcreerroff;
  int pcreov[120];
  char sfilter[SLEN+1];
  char afilter[SLEN+1];

  freedicts();
  atotal=0;
  r=0;
  ml=MEMBLK; // number of bytes stored so far in current memory block
  mp=NULL; // current memblk being filled
  dstrings=NULL; // linked list anchor
  for(dn=0;dn<MAXNDICTS;dn++) {
    if(dfnames[dn][0]=='\0') continue; // dictionary slot unused
    strcpy(sfilter,dsfilters[dn]);
    if(!strcmp(sfilter,"")) sre=0;
    else {
      sre=pcre_compile(sfilter,PCRE_CASELESS,&pcreerr,&pcreerroff,0);
      if(pcreerr) {
        sprintf(t,"Dictionary %d\nBad file filter syntax: %.100s",dn+1,pcreerr);
        reperr(t);
        }
      }
    strcpy(afilter,dafilters[dn]);
    if(!strcmp(afilter,"")) are=0;
    else {
      are=pcre_compile(afilter,PCRE_CASELESS,&pcreerr,&pcreerroff,0);
      if(pcreerr) {
        sprintf(t,"Dictionary %d\nBad answer filter syntax: %.100s",dn+1,pcreerr);
        reperr(t);
        }
      }
    for(mode=0;mode<2;mode++) { // try UTF-8 first (mode=0) then fall back to ISO 8859-1 (mode=1)
      r=0;
      f0=f1=f2=f3=0;
      if(!(fp=fopen(dfnames[dn],"r"))) {
        sprintf(t,"Dictionary %d:\nFile not found",dn+1);
        if(!sil) reperr(t);
        goto ew2;
        }
      DEB1 printf("Reading dictionary file %s (as %s)\n",dfnames[dn],mode?"ISO 8859-1":"UTF-8");
      for(;;) {
        if(feof(fp)) break;
        if(fgets(t,SLEN,fp)==NULL) break;
        j=strlen(t)-1;
        if(j<0) continue;
        while(j>=0&&t[j]>=0&&t[j]<=' ') t[j--]=0;  // UKACD has DOS line break; cope with this inter alia
        while (j>=0&&((t[j]>='0'&&t[j]<='9')||t[j]=='.'||t[j]=='+'||t[j]=='-')) j--; // get score (if any) from end
        j++;
        f=0.0;
        if(j==0||t[j-1]!=' ') j=strlen(t); // all digits, or no space? treat it as a word
        else {
          sscanf(t+j,"%f",&f);
          if(f>= 10.0) f= 10.0;
          if(f<=-10.0) f=-10.0;
          t[j--]=0; // rest of input is treated as a word (which may contain spaces)
          }
        while(j>=0&&t[j]>=0&&t[j]<=' ') t[j--]=0;  // remove trailing spaces, tabs etc.
        j++;
        if(j<1) continue;
        if(mode) for(i=0;i<=j;i++) ws[i]=(unsigned char)t[i]; // do ISO-8859-1 `conversion'
        else {
          memset(&ps,0,sizeof(ps));
          u=t;j=mbsrtowcs(ws,&u,SLEN,&ps); // do UTF-8 conversion
          if(j==-1) {DEB1 printf("File does not appear to be UTF-8 encoded\n");r=2;goto ew1;}
          if(j==SLEN) continue; // string too long: discard
          }
        ws[j]=0; // now have citation form as wide char string
        for(i=l1=0;i<j;i++) {
          m=ws[i];
          if(m<0||m>255) c='#'; else c=chmap[m];
          if(c=='#') {DEB1 printf("[%d=%02x]\n",m,m);f0=1;goto ew0;} // reject words with invalid characters
          if((c<'A'||c>'Z')&&(c<'0'||c>'9')) continue; // skip other non-alphanumeric
          if(m>127) f1=1; // remember if we saw any non-7-bit codes
          s1[l1++]=c; // copy to temporary string
          }
        s1[l1]=0; // terminate: now have untreated light form in s1
        if(l1==0) continue; // empty: skip
        if(l1>MXSZ) {f2=1;continue;} // too long: skip
        memset(&ps,0,sizeof(ps));
        wp=ws;
        l0=wcsrtombs(s0,&wp,SLEN*2,&ps); // now have citation form in s0
        if(l0==-1) {f3=1;continue;}
        if(sre) {
          i=pcre_exec(sre,0,s0,l0,0,0,pcreov,120);
          DEB1 if(i<-1) printf("PCRE error %d\n",i);
          if(i<0) continue; // failed match
          }
        if(are) {
          i=pcre_exec(are,0,s1,l1,0,0,pcreov,120);
          DEB1 if(i<-1) printf("PCRE error %d\n",i);
          if(i<0) continue; // failed match
          }

        if(ml+2+l0+1+l1+1>MEMBLK) { // allocate more memory if needed (this always happens on first pass round loop)
          q=(struct memblk*)malloc(sizeof(struct memblk));
          if(q==NULL) goto ew4;
          q->next=NULL;
          if(mp==NULL) dstrings=q; else mp->next=q; // link into list
          mp=q;
          mp->ct=0;
          ml=0;}
        *(mp->s+ml++)=(char)floor(f*10.0+128.5); // score with rounding
        *(mp->s+ml++)=dn;
        strcpy(mp->s+ml,s0);ml+=l0+1;
        strcpy(mp->s+ml,s1);ml+=l1+1;
        mp->ct++;atotal++; // count words of each length, words in this memblk, and total words

      ew0:;
        }
      DEB1 if(f0) printf("Discarded entries in dictionary file %s containing symbols\n",dfnames[dn]);
      DEB1 if(f1) printf("Removed accents from entries in dictionary file %s\n",dfnames[dn]);
      DEB1 if(f2) printf("Discarded entries in dictionary file %s more than %d letters long\n",dfnames[dn],MXSZ);
      DEB1 if(f3) printf("Discarded entries in dictionary file %s with erroneous UTF-8\n",dfnames[dn]);
      ew1:
      if(fp) fclose(fp);
      if(r==0) break; // read successfully
      }
    ew2:
    if(sre) pcre_free(sre);
    if(are) pcre_free(are);
    }

  if(r) {
    freedicts();
    return r;
    }
  // allocate array space from counts
  DEB1 printf("atotal=%9d\n",atotal);
  ans =(struct answer* )malloc(atotal*sizeof(struct answer));  if(ans ==NULL) goto ew4;
  ansp=(struct answer**)malloc(atotal*sizeof(struct answer*)); if(ansp==NULL) goto ew4; // pointer array for sorting

  p=dstrings;
  k=0;
  while(p!=NULL) {
    for(i=0,l=0;i<p->ct;i++) { // loop over all words
      ans[k].score=pow(10.0,((float)(*(unsigned char*)(p->s+l++))-128.0)/10.0);
      ans[k].cfdmask=
      ans[k].dmask=1<<*(unsigned char*)(p->s+l++);
      ans[k].cf   =p->s+l; l+=strlen(p->s+l)+1;
      ans[k].acf  =0;
      ans[k].ul   =p->s+l; l+=strlen(p->s+l)+1;
      k++;
      }
    p=p->next;
    }
  assert(k==atotal);
  for(i=0;i<atotal;i++) ansp[i]=ans+i;

    int cmpans(const void*p,const void*q) {int u; // string comparison for qsort
      u=strcmp( (*(struct answer**)p)->ul,(*(struct answer**)q)->ul); if(u) return u;
      u=strcmp( (*(struct answer**)p)->cf,(*(struct answer**)q)->cf);       return u;
      }

  qsort(ansp,atotal,sizeof(struct answer*),cmpans);
  ap=0; // ap points to first of each group of matching citation forms
  for(i=0,j=-1;i<atotal;i++) { // now remove duplicate entries
    if(i==0||strcmp(ansp[i]->ul,ansp[i-1]->ul)) {j++;ap=ansp[j]=ansp[i];}
    else {
      ansp[j]->dmask|=ansp[i]->dmask; // union masks
      ansp[j]->score*=ansp[i]->score; // multiply scores over duplicate entries
      if(strcmp(ansp[i]->cf,ansp[i-1]->cf)) ap->acf=ansp[i],ap=ansp[i]; // different citation forms? link them together
      else ap->cfdmask|=ansp[i]->cfdmask; // cf:s the same: union masks
      }
    }
  atotal=j+1;

  for(i=0;i<HTABSZ;i++) ahtab[i]=-1;
  for(i=0;i<atotal;i++) {
    h=0;
    for(j=0;ansp[i]->ul[j];j++) h=h*113+ansp[i]->ul[j];
    h=h%HTABSZ;
    ansp[i]->ahlink=ahtab[h];
    ahtab[h]=i;
    }

  for(i=0;i<atotal;i++) {
    if(ansp[i]->score>= 1e10) ansp[i]->score= 1e10; // clamp scores
    if(ansp[i]->score<=-1e10) ansp[i]->score=-1e10;
    }
  for(i=0;i<atotal;i++) {
    if(ansp[i]->acf==0) continue;
  //  printf("A%9d %08x %20s %5.2f",i,ansp[i]->dmask,ansp[i]->ul,ansp[i]->score);
    ap=ansp[i];
    do {
  //    printf(" \"%s\"",ap->cf);
      ap=ap->acf;
      } while(ap);
  //  printf("\n");
    }

  DEB1 printf("Total unique answers by entry: %d\n",atotal);

  aused=(char*)malloc(atotal*sizeof(char)); if(aused==NULL) goto ew4;
  return 0;
  ew4:
  reperr("Out of memory loading dictionaries");
  freedicts();
  return 4;
  }

void freedicts(void) { // free all memory allocated by loaddicts, even if it aborted mid-load
  struct memblk*p;
  while(dstrings) {p=dstrings->next;free(dstrings);dstrings=p;}
  FREEX(ans);
  FREEX(ansp);
  FREEX(aused);
  atotal=0;
  }

// INITIAL FEASIBLE LIST GENERATION

static int curans,curem,curten,curdm;

int treatmode;
char tpifname[SLEN];
char*treatmessage[NMSG];
char*treatmessageAZ[NMSG];
char treatmsg[NMSG][SLEN];
char treatmsgAZ[NMSG][SLEN];
int clueorderindex;
int lightlength;
int lightx;
int lighty;
int lightdir;
int tambaw;

// STATICS FOR VARIOUS TREATMENTS

// Playfair square
static char psq[25];
static int psc['Z'+1];

static int*tfl=0; // temporary feasible list
static int ctfl,ntfl;

static int clts;
static int hstab[HTABSZ];
static int haestab[HTABSZ];
static struct memblk*lstrings=0;
static struct memblk*lmp=0;
static int lml=MEMBLK;

// return index of light, creating if it doesn't exist; -1 on no memory
int findlight(const char*s,int a,int e) {
  unsigned int h0,h1;
  int i,u,l0;
  int l;
  struct light*p;
  struct memblk*q;

  h0=0;
  for(i=0;s[i];i++) h0=h0*113+s[i];
  h1=h0*113+a*27+e;
  h0=h0%HTABSZ; // h0 is hash of string only
  h1=h1%HTABSZ; // h1 is hash of string+treatment+entry method
  l=haestab[h1];
  while(l!=-1) {
    if(lts[l].ans==a&&lts[l].em==e&&!strcmp(s,lts[l].s)) return l; // exact hit in all particulars? return it
    l=lts[l].hashaeslink;
    }
  if(ltotal>=clts) { // out of space to store light structures? (always happens first time)
    clts=clts*2+5000; // try again a bit bigger
    p=realloc(lts,clts*sizeof(struct light));
    if(!p) return -1;
    lts=p;
    DEB2 printf("lts realloc: %d\n",clts);
    }
  l=hstab[h0];u=-1; // look for the light string, independent of how it arose
  while(l!=-1) {
    if(!strcmp(s,lts[l].s)) {u=lts[l].uniq;break;} // match found
    l=lts[l].hashslink;
    }
  if(u==-1) {
    l0=strlen(s)+1;
    if(lml+l0>MEMBLK) { // make space to store copy of light string
      DEB1 printf("memblk alloc\n");
      q=(struct memblk*)malloc(sizeof(struct memblk));
      if(!q) return -1;
      q->next=NULL;
      if(lmp==NULL) lstrings=q; else lmp->next=q; // link into list
      lmp=q;
      lml=0;
      }
    u=ultotal++; // allocate new uniquifying number
    lts[ltotal].s=lmp->s+lml;
    strcpy(lmp->s+lml,s);lml+=l0;
  } else {
    lts[ltotal].s=lts[l].s;
    }
  lts[ltotal].ans=a;
  lts[ltotal].em=e;
  lts[ltotal].uniq=u;
  lts[ltotal].hashslink  =hstab  [h0]; hstab  [h0]=ltotal; // insert into hash tables
  lts[ltotal].hashaeslink=haestab[h1]; haestab[h1]=ltotal;
  return ltotal++;
  }

// is word in dictionaries specified by curdm?
int isword(const char*s) {
  int i,p;
  unsigned int h;
  h=0;
  for(i=0;s[i];i++) h=h*113+s[i];
  h=h%HTABSZ;
  p=ahtab[h];
  while(p!=-1) {
    if(!strcmp(s,ansp[p]->ul)) return !!(ansp[p]->dmask&curdm);
    p=ansp[p]->ahlink;
    }
  return 0;
  }

// add light to feasible list: s=text of light, a=answer from which treated, e=entry method
// returns 0 if OK, !=0 on (out of memory) error
int addlight(const char*s,int a,int e) {
  int l;
  int*p;
  l=findlight(s,a,e);
  if(l<0) return l;
  if(ntfl>=ctfl) {
    ctfl=ctfl*3/2+500;
    p=realloc(tfl,ctfl*sizeof(int));
    if(!p) return -1;
    tfl=p;
    DEB2 printf("tfl realloc: %d\n",ctfl);
    }
  tfl[ntfl++]=l;
  return 0;
  }

// Add treated answer to feasible light list if suitable
// returns !=0 for error
int treatedanswer(const char*s) {
  char s0[MXSZ+1];
  int j,l,u;
  l=strlen(s);
  if(l!=lightlength) return 0;
  if(tambaw&&!isword(s)) return 0;
  if(curem&1) { // forwards entry method
    u=addlight(s,curans,0);if(u) return u;
    }
  if(curem&2) {
    for(j=0;j<l;j++) s0[j]=s[l-j-1]; // reversed entry method
    s0[j]=0;
    u=addlight(s0,curans,1);if(u) return u;
    }
  return 0;
  }

// returns !=0 on error
int inittreat(void) {int i,j,k;
  int c;

  for(i=0;i<NMSG;i++) {
    for(j=0,k=0;treatmsg[i][j];j++) { // make all capitals version
      if     (isupper(treatmsg[i][j])) treatmsgAZ[i][k++]=        treatmsg[i][j];
      else if(islower(treatmsg[i][j])) treatmsgAZ[i][k++]=toupper(treatmsg[i][j]);
      }
    treatmsgAZ[i][k]=0;
    }
  switch(treatmode) {
    case 1: // Playfair
      for(i='A';i<='Z';i++) psc[i]=-1; // generate code square
      for(i=0,k=0;treatmsgAZ[0][i]&&k<25;i++) {
        c=treatmsgAZ[0][i];
        if(c=='J') c='I';
        if(psc[c]==-1) {psc[c]=k;psq[k]=c;k++;}
        }
      for(c='A';c<='Z';c++) {
        if(c=='J') continue;
        if(psc[c]==-1) {psc[c]=k;psq[k]=c;k++;}
        }
      assert(k==25); // we should have filled in the whole square now
      psc['J']=psc['I'];
      DEB1 for(i=0;i<25;i++) {
        printf("%c ",psq[i]);
        if(i%5==4) printf("\n");
        }
      return 0;
    case 9: // custom plug-in
      for(i=0;i<NMSG;i++) {
        treatmessage[i]=treatmsg[i];
        treatmessageAZ[i]=treatmsgAZ[i];
        }
      break;
    default:break;
    }
  return 0;
  }

void finittreat(void) {}


void *tpih=0;
int (*treatf)(const char*)=0;

// returns error string or 0 for OK
char*loadtpi(void) {
  int (*f)(void);
  unloadtpi();
  dlerror(); // clear any existing error
  tpih=dlopen(tpifname,RTLD_LAZY);
  if(!tpih) return dlerror();
  dlerror();
  *(void**)(&f)=dlsym(tpih,"init"); // see man dlopen for the logic behind this
  if(!dlerror()) (*f)(); // initialise the plug-in
  *(void**)(&treatf)=dlsym(tpih,"treat");
  return dlerror();
  }

void unloadtpi(void) {void (*f)();
  if(tpih) {
    dlerror(); // clear any existing error
    *(void**)(&f)=dlsym(tpih,"finit");
    if(!dlerror()) (*f)(); // finalise it before unloading
    dlclose(tpih);
    }
  tpih=0;
  treatf=0;
  }

// returns !=0 on error
int treatans(const char*s) {
  int c0,c1,i,j,l,l0,l1,o,u;
  char t[MXSZ+1];
  l=strlen(s);
  // printf("treatans(%s)",s);fflush(stdout);
  for(i=0;s[i];i++) assert(s[i]>='A'&&s[i]<='Z');
  switch(treatmode) {
  case 0:return treatedanswer(s);
  case 1: // Playfair
    if(l!=lightlength) return 0;
    if(ODD(l)) return 0;
    for(i=0;i<l;i+=2) {
      c0=s[i];c1=s[i+1]; // letter pair to encode
      l0=psc[c0];l1=psc[c1]; // positions of chars in the square
      if(l0==l1) return 0; // don't handle double letters (including 'IJ' etc.)
      if     (l0/5==l1/5) t[i]=psq[ l0/5     *5+(l0+1)%5],t[i+1]=psq[ l1/5     *5+(l1+1)%5]; // same row
      else if(l0%5==l1%5) t[i]=psq[(l0/5+1)%5*5+ l0   %5],t[i+1]=psq[(l1/5+1)%5*5+ l1   %5]; // same col
      else                t[i]=psq[ l0/5     *5+ l1   %5],t[i+1]=psq[ l1/5     *5+ l0   %5]; // rectangle
      }
    t[i]=0;
    return treatedanswer(t);
  case 2: // substitution
    if(l!=lightlength) return 0;
    l0=strlen(treatmsgAZ[0]);
    for(i=0;s[i];i++) {
      j=s[i]-'A';
      if(j<l0) t[i]=treatmsgAZ[0][j];
      else     t[i]=s[i];
      }
    t[i]=0;
    return treatedanswer(t);
  case 3: // fixed Caesar/Vigenère
    if(l!=lightlength) return 0;
    l0=strlen(treatmsgAZ[0]);
    if(l0==0) return treatedanswer(s); // no keyword, so leave as plaintext
    for(i=0;s[i];i++) t[i]=(s[i]-'A'+treatmsgAZ[0][i%l0]-'A')%26+'A';
    t[i]=0;
    return treatedanswer(t);
  case 4: // variable Caesar
    if(l!=lightlength) return 0;
    l0=strlen(treatmsgAZ[0]);
    if(l0==0) return treatedanswer(s); // no keyword, so leave as plaintext
    o=treatmsgAZ[0][clueorderindex%l0]-'A';
    for(i=0;s[i];i++) t[i]=(s[i]-'A'+o)%26+'A';
    t[i]=0;
    return treatedanswer(t);
  case 5: // misprint
    if(l!=lightlength) return 0;
    l0=strlen(treatmsg[0]);
    l1=strlen(treatmsg[1]);
    if(clueorderindex>=l0&&clueorderindex>=l1) return treatedanswer(s);
    c0=clueorderindex<l0?treatmsg[0][clueorderindex]:'.';
    c1=clueorderindex<l1?treatmsg[1][clueorderindex]:'.';
    c0=toupper(c0);
    c1=toupper(c1);
    if(!isupper(c0)) c0='.';
    if(!isupper(c1)) c1='.';
    if(c0=='.'&&c1=='.') return treatedanswer(s); // allowing this case would slow things down too much for now
    strcpy(t,s);
    for(i=0;s[i];i++) if(c0=='.'||s[i]==c0) {
      if(c1=='.') {
        for(c1='A';c1<='Z';c1++) {
          if(s[i]==c1) continue; // not a *mis*print
          t[i]=c1;
          u=treatedanswer(t); if(u) return u;
          t[i]=s[i];
          }
      } else {
        if(c0=='.'&&s[i]==c1) continue; // not a *mis*print unless specifically instructed otherwise
        t[i]=c1;
        u=treatedanswer(t); if(u) return u;
        t[i]=s[i];
        if(c0==c1) break; // only one entry for the `misprint as self' case
        }
      }
    return 0;
  case 6: // delete single occurrence
    l0=strlen(treatmsg[0]);
    if(clueorderindex>=l0) return treatedanswer(s);
    if(l!=lightlength+1) return 0;
    c0=treatmsgAZ[0][clueorderindex];
    for(i=0;s[i];i++) if(s[i]==c0) {
      for(j=0;j<i;j++) t[j]=s[j];
      for(;s[j+1];j++) t[j]=s[j+1];
      t[j]=0;
      u=treatedanswer(t); if(u) return u;
      while(s[i+1]==c0) i++; // skip duplicate outputs
      }
    return 0;
  case 7: // delete all occurrences
    l0=strlen(treatmsg[0]);
    if(clueorderindex>=l0) return treatedanswer(s);
    if(l<=lightlength) return 0;
    c0=treatmsgAZ[0][clueorderindex];
    for(i=0,j=0;s[i];i++) if(s[i]!=c0) t[j++]=s[i];
    t[j]=0;
    if(j!=lightlength) return 0; // not necessary, but improves speed slightly
    return treatedanswer(t);
  case 8: // insert single letter
    l0=strlen(treatmsg[0]);
    if(clueorderindex>=l0) return treatedanswer(s);
    if(l!=lightlength-1) return 0;
    c0=treatmsgAZ[0][clueorderindex];
    for(i=0;i<=l;i++) {
      for(j=0;j<i;j++) t[j]=s[j];
      t[j++]=c0;
      for(;s[j-1];j++) t[j]=s[j-1];
      t[j]=0;
      u=treatedanswer(t); if(u) return u;
      while(s[i]==c0) i++; // skip duplicate outputs
      }
    break;
  case 9: // custom plug-in
    if(treatf) return (*treatf)(s);
    return 1;
  default:break;
    }
  return 0;
  }

extern int pregetinitflist(void) {
  int i;
  struct memblk*p;
  while(lstrings) {p=lstrings->next;free(lstrings);lstrings=p;} lmp=0; lml=MEMBLK;
  FREEX(tfl);ctfl=0;ntfl=0;
  FREEX(lts);clts=0;ltotal=0;ultotal=0;
  FREEX(lused);
  for(i=0;i<HTABSZ;i++) hstab[i]=-1,haestab[i]=-1;
  if (inittreat()) return 1;
  return 0;
  }

// returns !=0 for error
extern int postgetinitflist(void) {
  finittreat();
  FREEX(tfl);ctfl=0;
  FREEX(lused);
  lused=(char*)malloc(ultotal*sizeof(char));
  if(lused) return 0; else return -1;
  }

// construct an initial list of feasible lights for a given length etc.
// caller's responsibility to free(*l)
// returns !=0 on error
int getinitflist(int**l,int*ll,struct lprop*lp,int llen) {
  int i,u;
  ntfl=0;
  curdm=lp->dmask,curem=lp->emask,curten=lp->ten;
  lightlength=llen;
  DEB2 printf("getinitflist(%08x) llen=%d dmask=%08x emask=%08x ten=%d: ",(int)lp,llen,curdm,curem,curten);
  for(i=0;i<atotal;i++) {
    curans=i;
    if((curdm&ansp[curans]->dmask)==0) continue; // not in a valid dictionary
    if(curten) u=treatans(ansp[curans]->ul);
    else       u=treatedanswer(ansp[curans]->ul);
    if(u) return u;
    }
  *l=malloc(ntfl*sizeof(int));
  if(*l==0) return 1;
  memcpy(*l,tfl,ntfl*sizeof(int));
  *ll=ntfl;
  DEB2 printf("%d entries\n",ntfl);
  return 0;
  }
