// $Id: gui.c -1   $

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



// GTK

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <errno.h>

#include "common.h"
#include "qxw.h"
#include "filler.h"
#include "dicts.h"
#include "gui.h"
#include "draw.h"

int selmode;
int pxsq;
int zoomf=2;
int zoompx[]={18,26,36,52,72};
int curmf=1; // cursor has moved since last redraw

static char*gtypedesc[NGTYPE]={
"Plain rectangular","Hex with vertical lights","Hex with horizontal lights","Circular","Circular with half-cell offset"};

static int spropdia(int g);
static int lpropdia(int g);
static int dictldia(void);
static int treatdia(void);
static int gpropdia(void);
static int prefsdia(void);
static int statsdia(void);
static int filedia(char*but,char*ext);
static int box(int type,const char*t,const char*u,const char*v);
static void gridchangen(void);


GtkWidget *grid_da;
static GtkItemFactory *item_factory; // for menus
static GtkWidget *mainw,*grid_sw,*list_sw,*vbox,*hbox,*paned,*menubar,*poss_label,*clist; // main window and content
static GtkWidget*stats=NULL; // statistics window (if visible)
static GtkWidget*(st_te[MXSZ+2][5]),*(st_r[6]); // statistics table
static GtkWidget*currentdia; // for `Filling' dialogue box (so that we can close it automatically when filling completes)

// set main window title
static void setwintitle() {char t[SLEN*3];
  sprintf(t,"Qxw: %s",titlebyauthor());
  gtk_window_set_title(GTK_WINDOW(mainw),t);
  }

// set name of crossword and title bar of main window
void setbname(char*s) {
  strncpy(bname,s,SLEN-1);bname[SLEN-1]='\0';
  if(strlen(bname)>=4&&!strcmp(bname+strlen(bname)-4,".qxw")) bname[strlen(bname)-4]='\0';
  setwintitle();
  }





// GTK MENU HANDLERS

// simple menu handlers
static void m_filenew(GtkWidget*w,gpointer data)      {if(!unsaved||areyousure()) a_filenew();}
static void m_fileopen(GtkWidget*w,gpointer data)     {if(!unsaved||areyousure()) if(filedia("Open",".qxw")) a_load();}
static void m_filesave(GtkWidget*w,gpointer data)     {if(filedia("Save",".qxw")) a_save();}
static void m_filequit(void)                          {if(!unsaved||areyousure()) gtk_main_quit();}
static void m_undo(GtkWidget*w,gpointer data)         {if((uhead+UNDOS-1)%UNDOS==utail) return;undo_pop();gridchangen();syncgui();}
static void m_redo(GtkWidget*w,gpointer data)         {if(uhead==uhwm) return;uhead=(uhead+2)%UNDOS;undo_pop();gridchangen();syncgui();}
static void m_editgprop(GtkWidget*w,gpointer data)    {gpropdia();}
static void m_dsprop(GtkWidget*w,gpointer data)       {spropdia(1);}
static void m_sprop(GtkWidget*w,gpointer data)        {spropdia(0);}
static void m_dlprop(GtkWidget*w,gpointer data)       {lpropdia(1);}
static void m_lprop(GtkWidget*w,gpointer data)        {lpropdia(0);}
static void m_afctreat(GtkWidget*w,gpointer data)     {treatdia();}
static void m_editprefs(GtkWidget*w,gpointer data)    {prefsdia();}
static void m_showstats(GtkWidget*w,gpointer data)    {statsdia();}
static void m_symm0(GtkWidget*w,gpointer data)        {symmr=(int)data&0xff;}
static void m_symm1(GtkWidget*w,gpointer data)        {symmm=(int)data&0xff;}
static void m_symm2(GtkWidget*w,gpointer data)        {symmd=(int)data&0xff;}

static void m_zoom(GtkWidget*w,gpointer data) {int u;
  u=(int)data;
  if(u==-1) zoomf++;
  else if(u==-2) zoomf--;
  else zoomf=u;
  if(zoomf<0) zoomf=0;
  if(zoomf>4) zoomf=4;
  pxsq=zoompx[zoomf];
  syncgui();
  }

static void m_fileexport(GtkWidget*w,gpointer data)  {
  switch((int)data) {
  case 0x401: if(filedia("Export blank grid as EPS",".blank.eps")) a_exportg(filename,4,0);break;
  case 0x402: if(filedia("Export blank grid as PNG",".blank.png")) a_exportg(filename,4,1);break;
  case 0x403:
    if(gtype!=0) break;
    if(filedia("Export blank grid as HTML",".blank.html")) a_exportgh(0x05,"");
    break;
  case 0x411: if(filedia("Export filled grid as EPS",".eps")) a_exportg(filename,2,0);break;
  case 0x412: if(filedia("Export filled grid as PNG",".png")) a_exportg(filename,2,1);break;
  case 0x413:
    if(gtype!=0) break;
    if(filedia("Export filled grid as HTML",".html")) a_exportgh(0x03,"");
    break;
  case 0x420: if(filedia("Export answers as text",".ans.txt")) a_exporta(0);break;
  case 0x423: if(filedia("Export answers as HTML",".ans.html")) a_exporta(1);break;
  case 0x433:
    if(gtype!=0) break;
    if(filedia("Export puzzle as HTML",".html")) a_exportgh(0x0d,"");
    break;
  case 0x434:if(filedia("Export puzzle as HTML+PNG",".html")) a_exporthp(0);break;
  case 0x443:
    if(gtype!=0) break;
    if(filedia("Export solution as HTML",".html")) a_exportgh(0x13,"");
    break;
  case 0x444:if(filedia("Export solution as HTML+PNG",".html")) a_exporthp(1);break;
  default: assert(0);
    }
  }

void selchange(void) {refreshsel();}

void selcell(int x,int y,int f) {int i,l,gx[MXSZ],gy[MXSZ];
  l=getmergegroup(gx,gy,x,y);
  if(f) f=16;
  for(i=0;i<l;i++) gsq[gx[i]][gy[i]].fl=(gsq[gx[i]][gy[i]].fl&~16)|f;
  }

static void selnocells() {int i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) gsq[i][j].fl&=~16;
  }

static void selnolights() {int i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) gsq[i][j].dsel=0;
  }

// clear selection
static void m_selnone(GtkWidget*w,gpointer data) {
  selnolights();
  selnocells();
  selchange();
  }

// invert selection
static void m_selinv(GtkWidget*w,gpointer data) {int d,i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) {
    if(selmode==0) {if(isclear(i,j)) gsq[i][j].fl^=16;}
    else           for(d=0;d<ndir[gtype];d++) if(isstartoflight(i,j,d)) gsq[i][j].dsel^=1<<d;
    }
  selchange();
  }

// select everything
static void m_selall(GtkWidget*w,gpointer data) {int d,i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) {
    if(selmode==0) {if(isclear(i,j)) gsq[i][j].fl|=16;}
    else           for(d=0;d<ndir[gtype];d++) if(isstartoflight(i,j,d)) gsq[i][j].dsel|=1<<d;
    }
  selchange();
  }

// switch to light mode
static void sel_tol(void) {int d,i,j;
  if(selmode==1) return;
  DEB1 printf("seltol()\n");
  selnolights();
  for(i=0;i<width;i++) for(j=0;j<height;j++) if(gsq[i][j].fl&16) for(d=0;d<ndir[gtype];d++) {
    DEB1 printf("  %d %d %d\n",i,j,d);
    sellight(i,j,d,1);
    }
  selmode=1;
  }

// switch to cell mode
static void sel_toc(void) {int i,j,d,k,l,m,n,gx[MXSZ],gy[MXSZ],lx[MXSZ],ly[MXSZ];
  DEB1 printf("seltoc()\n");
  if(selmode==0) return;
  selnocells();
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)&&issellight(i,j,d)) {
    DEB1 printf("  %d %d %d\n",i,j,d);
    l=getlight(lx,ly,i,j,d);
    for(k=0;k<l;k++) {
      m=getmergegroup(gx,gy,lx[k],ly[k]);
      for(n=0;n<m;n++) gsq[gx[n]][gy[n]].fl|=16;
      }
    }
  selmode=0;
  }

// selection commands: single light
static void m_sellight(GtkWidget*w,gpointer data) {
  if(selmode==0) m_selnone(w,data);
  selmode=1;
  sellight(curx,cury,dir,!issellight(curx,cury,dir));
  selchange();
  }

// all lights parallel
static void m_sellpar(GtkWidget*w,gpointer data) {int f,i,j;
  if(selmode==0) m_selnone(w,data);
  selmode=1;
  f=issellight(curx,cury,dir);
  for(i=0;i<width;i++) for(j=0;j<height;j++) if(isstartoflight(i,j,dir)) sellight(i,j,dir,!f);
  selchange();
  }

// single cell
static void m_selcell(GtkWidget*w,gpointer data) {int f;
  if(selmode==1) m_selnone(w,data);
  selmode=0;
  if(!isclear(curx,cury)) return;
  f=(~gsq[curx][cury].fl)&16;
  selcell(curx,cury,f);
  selchange();
  }

static void m_selcover(GtkWidget*w,gpointer data) {int i,j;
  if(selmode==1) m_selnone(w,data);
  selmode=0;
  for(i=0;i<width;i++) for(j=0;j<height;j++)
    if(isownmergerep(i,j)) selcell(i,j,gsq[i][j].sp.spor);
  selchange();
  }

// static void m_selctreat(GtkWidget*w,gpointer data) {int i,j;
//   if(selmode==1) m_selnone(w,data);
//   selmode=0;
//   for(i=0;i<width;i++) for(j=0;j<height;j++)
//     if(isownmergerep(i,j)) selcell(i,j,gsq[i][j].sp.spor?gsq[i][j].sp.ten:dsp.ten);
//   selchange();
//   }

static void m_sellover(GtkWidget*w,gpointer data) {int d,i,j;
  if(selmode==0) m_selnone(w,data);
  selmode=1;
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,gsq[i][j].lp[d].lpor);
  selchange();
  }

static void m_selltreat(GtkWidget*w,gpointer data) {int d,i,j;
  if(selmode==0) m_selnone(w,data);
  selmode=1;
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,gsq[i][j].lp[d].lpor?gsq[i][j].lp[d].ten:dlp.ten);
  selchange();
  }

// select violating lights
static void m_selviol(GtkWidget*w,gpointer data) {int d,i,j,v;
  if(selmode==0) m_selnone(w,data);
  selmode=1;
  v=(int)data;
  DEB1 printf("selviol(%d)\n",v);
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,!!(gsq[i][j].vflags[d]&v));
  selchange();
  }

static void m_selmode(GtkWidget*w,gpointer data) {
  if(selmode==0) sel_tol();
  else           sel_toc();
  selchange();
  }

void curmoved(void) {
  refreshcur();
  curmf=1;
  }

void gridchangen(void) {
  int d,i,j;
  // only squares at start of lights can have dsel information
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++) if(!isstartoflight(i,j,d)) gsq[i][j].dsel&=~(1<<d);
  selchange();
  curmoved();
  donumbers();
  refreshnum();
  compute();
  }

void gridchange(void) {
  gridchangen();
  undo_push();
  }

static void m_erase(void) {int i,j;
  if(selmode==1) {
    sel_toc();
    selchange();
    }
  for(i=0;i<width;i++) for(j=0;j<height;j++) {if(gsq[i][j].fl&16) {gsq[i][j].ch=' ';refreshsqmg(i,j);}}
  gridchange();
  }

static void m_dictionaries(GtkWidget*w,gpointer data) {dictldia();}

// convert tentative entries (where feasible letter bitmap has exactly one bit set) to firm entries
static void m_accept(GtkWidget*w,gpointer data) {int i,j;
  DEB1 printf("m_accept\n");
  for(i=0;i<width;i++) for(j=0;j<height;j++)
    if(gsq[i][j].e) {
      if(cbits(gsq[i][j].flbm)==1) {gsq[i][j].ch=ltochar[logbase2(gsq[i][j].flbm)];refreshsqmg(i,j);}
      }
  gridchange();
  }

// run filler
static void m_autofill(GtkWidget*w,gpointer data) {
  if(fillmode) return; // already running?
  fillmode=(int)data; // selected mode (1=all, 2=selection)
  if(fillmode==2&&selmode==1) {
    sel_toc();
    selchange();
    }
  compute(); // start
  box(GTK_MESSAGE_INFO,"\n  Filling in progress  \n","  Stop  ",0); // this box is closed manually or by the filler when it completes
  fillmode=0; // filling done
  }

// grid edit operations

static void editblock (int x,int y,int d) {               symmdo(a_editblock ,0,x,y,d);gridchange();}
static void editspace (int x,int y,int d) {setch(x,y,' ');symmdo(a_editspace ,0,x,y,d);gridchange();}
static void editcutout(int x,int y,int d) {               symmdo(a_editcutout,0,x,y,d);gridchange();}

// called from menu
static void m_editblock (GtkWidget*w,gpointer data) {editblock (curx,cury,dir);}
static void m_editspace (GtkWidget*w,gpointer data) {editspace (curx,cury,dir);}
static void m_editcutout(GtkWidget*w,gpointer data) {editcutout(curx,cury,dir);}

// bar behind cursor
static void m_editbarb(GtkWidget*w,gpointer data) {
  symmdo(a_editbar,!isbar(curx,cury,dir+ndir[gtype]),curx,cury,dir+ndir[gtype]);
  gridchange();
  }

// merge/join ahead
static void m_editmerge(GtkWidget*w,gpointer data) {
  symmdo(a_editmerge,!ismerge(curx,cury,dir),curx,cury,dir);
  gridchange();
  }



// flip grid in main diagonal
static void m_editflip(GtkWidget*w,gpointer data) {int i,j,k,t;struct square s;struct lprop l;
  if(gtype>2) return;
  for(i=0;i<MXSZ;i++) for(j=i+1;j<MXSZ;j++) {s=gsq[j][i];gsq[j][i]=gsq[i][j];gsq[i][j]=s;}
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) {
    t=gsq[i][j].bars;
    if(gtype==0) t=((t&1)<<1)|((t&2)>>1); // swap bars around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].bars=t;
    t=gsq[i][j].merge;
    if(gtype==0) t=((t&1)<<1)|((t&2)>>1); // swap merges around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].merge=t;
    t=gsq[i][j].dsel;
    if(gtype==0) t=((t&1)<<1)|((t&2)>>1); // swap directional selects around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].dsel=t;
    k=(gtype==0)?1:2;
    l=gsq[i][j].lp[0];gsq[i][j].lp[0]=gsq[i][j].lp[k];gsq[i][j].lp[k]=l;
    }
  t=height;height=width;width=t; // exchange width and height
  if(symmm==1||symmm==2) symmm=3-symmm; // flip symmetries
  if(symmd==1||symmd==2) symmd=3-symmd;
  if(gtype==1||gtype==2) gtype=3-gtype;
  gridchange();
  selchange();
  syncgui();
  }

// rotate (circular) grid
static void m_editrot(GtkWidget*w,gpointer data) {int i,j; struct square t,u;
  if(gtype<3) return;
  if((int)data==0xf001)
    for(j=0;j<height;j++) {t=gsq[width-1][j];for(i=      0;i<width;i++) u=gsq[i][j],gsq[i][j]=t,t=u;}
  else
    for(j=0;j<height;j++) {t=gsq[      0][j];for(i=width-1;i>=0   ;i--) u=gsq[i][j],gsq[i][j]=t,t=u;}
  gridchange();
  selchange();
  syncgui();
  }

static void m_helpabout(GtkWidget*w,gpointer data) {char s[2560];
  sprintf(s,
  "%s\n\n"
  "Copyright 2011 Mark Owen\n"
  "\n"
  "This program is free software; you can redistribute it and/or modify "
  "it under the terms of version 2 of the GNU General Public License as "
  "published by the Free Software Foundation.\n"
  "\n"
  "This program is distributed in the hope that it will be useful, "
  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
  "GNU General Public License for more details.\n"
  "\n"
  "You should have received a copy of the GNU General Public License along "
  "with this program; if not, write to the Free Software Foundation, Inc., "
  "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n"
  "\n"
  "For more information visit http://www.quinapalus.com/ or "
  "contact qxw@quinapalus.com\n"
  ,RELEASE);
  okbox(s);}

static int w_destroy(void) {gtk_main_quit();return 0;}
static int w_delete(void) {return !(!unsaved||areyousure());}






static void moveleft(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:(*x)--;break;
    case 3:case 4:
      *x=(*x+width-1)%width;
      break;
    }
  }

static void moveright(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:(*x)++;break;
    case 3:case 4:
      *x=(*x+1)%width;
      break;
    }
  }

static void moveup(int*x,int*y)   {(*y)--;}
static void movedown(int*x,int*y) {(*y)++;}

static void movehome(int*x,int*y) {int l,lx[MXSZ],ly[MXSZ];
  l=getlight(lx,ly,*x,*y,dir);
  if(l<1) return;
  *x=lx[0];*y=ly[0];
  }

static void moveend(int*x,int*y) {int l,lx[MXSZ],ly[MXSZ];
  l=getlight(lx,ly,*x,*y,dir);
  if(l<1) return;
  *x=lx[l-1];*y=ly[l-1];
  }



// GTK EVENT HANDLERS

// process keyboard
static int keypress(GtkWidget *widget, GdkEventKey *event) {int k,f,r,x,y;
  r=0; // flag whether we have processed this key
  k=event->keyval; f=event->state;
  if(k>='a'&&k<='z') k+='A'-'a';
  DEB1 printf("keypress event: %x %x\n",k,f);
  f&=12; // mask off flags except CTRL, ALT
  if((k==GDK_Tab&&f==0)||(((k>='A'&&k<='Z')||(k>='0'&&k<='9'))&&f==0)) { // tab and letters, digits
    r=1;
    setch(curx,cury,(k==GDK_Tab)?' ':k);
    refreshsqmg(curx,cury);
    stepforwmifingrid(&curx,&cury,dir);
    undo_push();
    }
  if(k==' '&&f==0)       {r=1;stepforwmifingrid(&curx,&cury,dir);}
  if(k==GDK_Left &&f==0) {r=1; x=curx;y=cury;moveleft (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
  if(k==GDK_Right&&f==0) {r=1; x=curx;y=cury;moveright(&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
  if(k==GDK_Up   &&f==0) {r=1; x=curx;y=cury;moveup   (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
  if(k==GDK_Down &&f==0) {r=1; x=curx;y=cury;movedown (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}

  if(k==GDK_Home &&f==0) {r=1; x=curx;y=cury;movehome (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
  if(k==GDK_End  &&f==0) {r=1; x=curx;y=cury;moveend  (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}

  if(k==GDK_Page_Up   &&f==0) {r=1; dir=(dir+ndir[gtype]-1)%ndir[gtype];}
  if(k==GDK_Page_Down &&f==0) {r=1; dir=(dir+            1)%ndir[gtype];}

  if(k==GDK_BackSpace&&f==0) {r=1; stepbackifingrid(&curx,&cury,dir);}
  if(r) { // if we have processed the key, update both old and new cursor positions
    curmoved();
    compute();
    }
  return r;
  }

// convert screen coords to internal square coords; k is bitmap of sufficiently nearby edges
void ptrtosq(int*x,int*y,int*k,int x0,int y0) {float u,v,u0,v0,r=0,t=0,xa=0,ya=0,xb=0,yb=0;int i,j;
  u0=((float)x0-bawdpx)/pxsq;v0=((float)y0-bawdpx)/pxsq;
  *k=0;
  switch(gtype) {
  case 0:
    i=floor(u0);u=u0-i;
    j=floor(v0);v=v0-j;
    *x=i;*y=j;
    break;
  case 1:
    i=(int)floor(u0/1.2);u=u0-i*1.2;
    if((i&1)==0) {
      j=(int)floor(v0/1.4);v=v0-j*1.4;
      if(u>=0.4) {*x=i;*y=j;break;}
      if( 0.7*u+0.4*v<0.28) {*x=i-1;*y=j-1;break;}
      if(-0.7*u+0.4*v>0.28) {*x=i-1;*y=j;  break;}
      *x=i;*y=j;break;
    } else {
      j=(int)floor((v0-0.7)/1.4);v=v0-0.7-j*1.4;
      if(u>=0.4) {*x=i;*y=j;break;}
      if( 0.7*u+0.4*v<0.28) {*x=i-1;*y=j;  break;}
      if(-0.7*u+0.4*v>0.28) {*x=i-1;*y=j+1;break;}
      *x=i;*y=j;break;
      }
  case 2:
    j=(int)floor(v0/1.2);v=v0-j*1.2;
    if((j&1)==0) {
      i=(int)floor(u0/1.4);u=u0-i*1.4;
      if(v>=0.4) {*y=j;*x=i;break;}
      if( 0.7*v+0.4*u<0.28) {*y=j-1;*x=i-1;break;}
      if(-0.7*v+0.4*u>0.28) {*y=j-1;*x=i;  break;}
      *y=j;*x=i;break;
    } else {
      i=(int)floor((u0-0.7)/1.4);u=u0-0.7-i*1.4;
      if(v>=0.4) {*y=j;*x=i;break;}
      if( 0.7*v+0.4*u<0.28) {*y=j-1;*x=i;  break;}
      if(-0.7*v+0.4*u>0.28) {*y=j-1;*x=i+1;break;}
      *y=j;*x=i;break;
      }
  case 3:case 4:
    u=u0-height;v=v0-height;
    r=sqrt(u*u+v*v);
    if(r<1e-3) {*x=-1;*y=-1;return;}
    r=height-r;
    t=atan2(u,-v)*width/2/PI;
    if(gtype==4) t+=.5;
    if(t<0) t+=width;
    *x=(int)floor(t);
    *y=(int)floor(r);
    break;
    }
  switch(gtype) { // click-near-edge bitmap
  case 0:case 1:case 2:
    for(i=0;i<ndir[gtype]*2;i++) {
      edgecoords(&xa,&ya,&xb,&yb,*x,*y,i);
      xb-=xa;yb-=ya;
      r=sqrt(xb*xb+yb*yb);xb/=r;yb/=r;
      xa=u0-xa;ya=v0-ya;
      if(fabs(xb*ya-xa*yb)<PXEDGE) *k|=1<<i;
      }
    break;
  case 3:case 4:
    if(r-*y>1-PXEDGE            ) *k|=2;
    if(r-*y<  PXEDGE            ) *k|=8;
    r=height-r;
    if(t-*x>1-PXEDGE/((r<2)?1:(r-1))) *k|=1;
    if(t-*x<  PXEDGE/((r<2)?1:(r-1))) *k|=4;
    break;
    }
  }

int dragflag=-1; // store selectedness state of square where shift-drag starts, or -1 if none in progress

static void mousel(int x,int y) { // left button
  if(selmode==1) m_selnone(0,0);
  selmode=0;
  if(dragflag==-1) dragflag=gsq[x][y].fl&16; // set f if at beginning of shift-drag
  if((gsq[x][y].fl&16)==dragflag) {
    selcell(x,y,!dragflag);
    selchange();
    }
  }

static void mouser(int x,int y) { // right button
  if(selmode==0) m_selnone(0,0);
  selmode=1;
  if(dragflag==-1) dragflag=issellight(x,y,dir);
  if(issellight(x,y,dir)==dragflag) {
    sellight(x,y,dir,!dragflag);
    selchange();
    }
  }

// pointer motion
static gint mousemove(GtkWidget*widget,GdkEventMotion*event) {
  int e,k,x,y;
  ptrtosq(&x,&y,&e,event->x,event->y);
  k=(int)(event->state); // buttons and modifiers
  DEB2 printf("mouse move event (%d,%d) %d e=%02x\n",x,y,k,e);
  if(!isingrid(x,y)) return 0;
  if((k&GDK_SHIFT_MASK)==0) {dragflag=-1;return 0;} // shift not held down: reset f
  if     (k&GDK_BUTTON3_MASK) mouser(x,y);
  else if(k&GDK_BUTTON1_MASK) mousel(x,y);
  else dragflag=-1; // button not held down: reset dragflag
  return 0;
  }

// click in grid area
static gint button_press_event(GtkWidget*widget,GdkEventButton*event) {
  int b,e,ee,k,x,y;
  ptrtosq(&x,&y,&e,event->x,event->y);
  k=event->state;
  DEB2 printf("button press event (%f,%f) -> (%d,%d) e=%02x button=%08x type=%08x state=%08x\n",event->x,event->y,x,y,e,(int)event->button,(int)event->type,k);
  if(event->type==GDK_BUTTON_RELEASE) dragflag=-1;
  if(event->type!=GDK_BUTTON_PRESS) goto ew0;
  if(k&GDK_SHIFT_MASK) {
    if     (event->button==3) mouser(x,y);
    else if(event->button==1) mousel(x,y);
    return 0;
    }
  if(event->button!=1) goto ew0; // only left clicks do anything now
  if(!isingrid(x,y)) goto ew0;
  ee=!!(e&(e-1)); // more than one bit set in e?

  if(clickblock&&ee) { // flip between block and space when a square is clicked near a corner
    if((gsq[x][y].fl&1)==0) {editblock(x,y,dir);gridchange();goto ew1;}
    else                    {editspace(x,y,dir);gridchange();goto ew1;}
    }
  else if(clickbar&&e&&!ee) { // flip bar if clicked in middle of edge
    b=logbase2(e);
    DEB2 printf("  x=%d y=%d e=%d b=%d isbar(x,y,b)=%d\n",x,y,e,b,isbar(x,y,b));
    symmdo(a_editbar,!isbar(x,y,b),x,y,b);
    gridchange();
    goto ew1;
    }

  if(x==curx&&y==cury) dir=(dir+1)%ndir[gtype]; // flip direction for click in square of cursor
  curx=x; cury=y; // not trapped as producing bar or block, so move cursor
  curmoved();
  gridchange();

  ew1:
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  ew0:
  DEB2 printf("Exiting button_press_event()\n");
  return FALSE;
  }

// word list entry selected
static void selrow(GtkWidget*widget,gint row,gint column, GdkEventButton*event,gpointer data) {
  int i,l,lx[MXSZ],ly[MXSZ];
  DEB1 printf("row select event\n");
  l=getlight(lx,ly,curx,cury,dir);
  if(l<2) return;
  if(!llist) return;
  if(row<0||row>=llistn) return;
  for(i=0;i<l;i++) {setch(lx[i],ly[i],lts[llistp[row]].s[i]);refreshsqmg(lx[i],ly[i]);}
  gridchange();
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  }

// grid update
static gint configure_event(GtkWidget*widget,GdkEventConfigure*event) {
  DEB4 printf("config event: new w=%d h=%d\n",widget->allocation.width,widget->allocation.height);
  if(widget==grid_da) {
    }
  return TRUE;
  }

// redraw the screen from the backing pixmap
static gint expose_event(GtkWidget*widget,GdkEventExpose*event) {float u,v;
  cairo_t*cr;
  DEB4 printf("expose event x=%d y=%d w=%d h=%d\n",event->area.x,event->area.y,event->area.width,event->area.height),fflush(stdout);
  if(widget==grid_da) {
    cr=gdk_cairo_create(widget->window);
    cairo_rectangle(cr,event->area.x,event->area.y,event->area.width,event->area.height);
    cairo_clip(cr);
    repaint(cr);
    cairo_destroy(cr);
    if(curmf) {
      mgcentre(&u,&v,curx,cury,0,1);
      DEB1 printf("curmoved: %f %f %d %d\n",u,v,pxsq,bawdpx);
      gtk_adjustment_clamp_page(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(grid_sw)),(u-.5)*pxsq+bawdpx-3,(u+.5)*pxsq+bawdpx+3); // scroll window to follow cursor
      gtk_adjustment_clamp_page(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(grid_sw)),(v-.5)*pxsq+bawdpx-3,(v+.5)*pxsq+bawdpx+3);
      curmf=0;
      }
    }
  return FALSE;
  }

void invaldarect(int x0,int y0,int x1,int y1) {GdkRectangle r;
  r.x=x0; r.y=y0; r.width=x1-x0; r.height=y1-y0;
  DEB4 printf("invalidate(%d,%d - %d,%d)\n",x0,y0,x1,y1);
  gdk_window_invalidate_rect(grid_da->window,&r,0);
  }

void invaldaall() {
  DEB4 printf("invalidate all\n");
  gdk_window_invalidate_rect(grid_da->window,0,0);
  }





// GTK DIALOGUES

// general filename dialogue: button text in but, default file extension in ext
static int filedia(char*but,char*ext) {
  GtkWidget*dia;
  int i;
  dia=gtk_file_selection_new(but);
  strcpy(filename,bname);strcat(filename,ext);
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(dia),filename);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  i=i==GTK_RESPONSE_OK;
  if(i) strcpy(filename,gtk_file_selection_get_filename((GtkFileSelection*)dia));
  else strcpy(filename,"");
  gtk_widget_destroy(dia);
  return i; // return 1 if got name successfully (although may be empty string)
  }

// square properties dialogue
static int spropdia(int g) {
  GtkWidget*dia,*cbg,*cfg,*or,*l0,*l1,*w,*vb; // *tr;
  GdkColor gcbg,gcfg;
  struct sprop*sps[MXSZ*MXSZ];
  int nsps;
  int i,j;

  nsps=0;
  if(g) {sps[0]=&dsp;nsps=1;}
  else {
    for(j=0;j<height;j++) for(i=0;i<width;i++) if(gsq[i][j].fl&16) sps[nsps++]=&gsq[i][j].sp;
    if(selmode==1||nsps==0) {reperr("Please select one or more cells\nwhose properties you wish to change");return 1;}
    }
  i=sps[0]->bgcol;
  gcbg.red  =((i>>16)&255)*257;
  gcbg.green=((i>> 8)&255)*257;
  gcbg.blue =((i    )&255)*257;
  i=sps[0]->fgcol;
  gcfg.red  =((i>>16)&255)*257;
  gcfg.green=((i>> 8)&255)*257;
  gcfg.blue =((i    )&255)*257;

  dia=gtk_dialog_new_with_buttons(g?"Default cell properties":"Selected cell properties",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

    int setactive() {int k;
      k=g||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(or));
      gtk_widget_set_sensitive(cbg,k);
      gtk_widget_set_sensitive(cfg,k);
      gtk_widget_set_sensitive(l0,k);
      gtk_widget_set_sensitive(l1,k);
  //    gtk_widget_set_sensitive(tr,k);
      return 1;
      }

  if(!g) {
    or=gtk_check_button_new_with_mnemonic("Override default cell properties");
    gtk_box_pack_start(GTK_BOX(vb),or,TRUE,TRUE,0);
    gtk_signal_connect(GTK_OBJECT(or),"clicked",GTK_SIGNAL_FUNC(setactive),0);
    w=gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
    }

  w=gtk_hbox_new(0,2);
  l0=gtk_label_new("Background colour: ");
  gtk_label_set_width_chars(GTK_LABEL(l0),18);
  gtk_misc_set_alignment(GTK_MISC(l0),1,0.5);
  gtk_box_pack_start(GTK_BOX(w),l0,FALSE,FALSE,0);
  cbg=gtk_color_button_new_with_color(&gcbg);
  gtk_box_pack_start(GTK_BOX(w),cbg,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w=gtk_hbox_new(0,2);
  l1=gtk_label_new("Foreground colour: ");
  gtk_label_set_width_chars(GTK_LABEL(l1),18);
  gtk_misc_set_alignment(GTK_MISC(l1),1,0.5);
  gtk_box_pack_start(GTK_BOX(w),l1,FALSE,FALSE,0);
  cfg=gtk_color_button_new_with_color(&gcfg);
  gtk_box_pack_start(GTK_BOX(w),cfg,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  // w=gtk_hseparator_new();
  // gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  // tr=gtk_check_button_new_with_mnemonic("_Enable cell treatment");
  // gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tr),sps[0]->ten);
  // gtk_box_pack_start(GTK_BOX(vb),tr,TRUE,TRUE,0);

  if(!g) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(or),(sps[0]->spor)&1);
  setactive();

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    gtk_color_button_get_color(GTK_COLOR_BUTTON(cbg),&gcbg);
    gtk_color_button_get_color(GTK_COLOR_BUTTON(cfg),&gcfg);
    for(j=0;j<nsps;j++) {
      if(!g) sps[j]->spor=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(or));
      sps[j]->bgcol=
        (((gcbg.red  >>8)&255)<<16)+
        (((gcbg.green>>8)&255)<< 8)+
        (((gcbg.blue >>8)&255)    );
      sps[j]->fgcol=
        (((gcfg.red  >>8)&255)<<16)+
        (((gcfg.green>>8)&255)<< 8)+
        (((gcfg.blue >>8)&255)    );
  //    sps[j]->ten=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tr));
      }
    }
  gtk_widget_destroy(dia);
  gridchange();
  refreshall();
  return 1;
  }


// light properties dialogue
static int lpropdia(int g) {
  GtkWidget*dia,*e[MAXNDICTS],*f[NLEM],*l,*w,*vb,*tr;
  struct lprop*lps[MXSZ*MXSZ*MAXNDIR];
  int nlps;
  int d,i,j;
  char s[SLEN];

  nlps=0;
  if(g) {lps[0]=&dlp;nlps=1;}
  else {
    for(d=0;d<ndir[gtype];d++) for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)&&issellight(i,j,d)) lps[nlps++]=&gsq[i][j].lp[d];
    if(selmode==0||nlps==0) {reperr("Please select one or more lights\nwhose properties you wish to change");return 1;}
    }
  dia=gtk_dialog_new_with_buttons(g?"Default light properties":"Selected light properties",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

    int setactive() {int i,k;
      k=g||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l));
      for(i=0;i<MAXNDICTS;i++) gtk_widget_set_sensitive(e[i],k);
      for(i=0;i<NLEM;i++)   gtk_widget_set_sensitive(f[i],k);
      gtk_widget_set_sensitive(tr,k);
      return 1;
      }

  if(!g) {
    l=gtk_check_button_new_with_mnemonic("Override default light properties");
    gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
    gtk_signal_connect(GTK_OBJECT(l),"clicked",GTK_SIGNAL_FUNC(setactive),0);
    w=gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
    }
  for(i=0;i<MAXNDICTS;i++) {
    sprintf(s,"Use dictionary _%d (",i+1);
    if(strlen(dfnames[i])>15) {strcat(s,"...");strcat(s,dfnames[i]+strlen(dfnames[i])-12);}
    else if(strlen(dfnames[i])==0) strcat(s,"<empty>");
    else strcat(s,dfnames[i]);
    strcat(s,")");
    e[i]=gtk_check_button_new_with_mnemonic(s);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e[i]),((lps[0]->dmask)>>i)&1);
    gtk_box_pack_start(GTK_BOX(vb),e[i],TRUE,TRUE,0);
    }
  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  tr=gtk_check_button_new_with_mnemonic("_Enable answer treatment");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tr),lps[0]->ten);
  gtk_box_pack_start(GTK_BOX(vb),tr,TRUE,TRUE,0);
  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  for(i=0;i<NLEM;i++) {
    sprintf(s,"Allow light to be entered _%s",lemdescADVP[i]);
    f[i]=gtk_check_button_new_with_mnemonic(s);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(f[i]),((lps[0]->emask)>>i)&1);
    gtk_box_pack_start(GTK_BOX(vb),f[i],TRUE,TRUE,0);
    }
  if(!g) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l),(lps[0]->lpor)&1);
  setactive();
  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) for(j=0;j<nlps;j++) {
    if(!g) lps[j]->lpor=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l));
    lps[j]->dmask=0;
    for(i=0;i<MAXNDICTS;i++) if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e[i]))) lps[j]->dmask|=1<<i;
    lps[j]->emask=0;
    for(i=0;i<NLEM;i++) if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(f[i]))) lps[j]->emask|=1<<i;
    lps[j]->ten=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tr));
    }
  gtk_widget_destroy(dia);
  gridchange();
  return 1;
  }

// treatment dialogue
static int treatdia(void) {
  GtkWidget*dia,*e[NMSG],*f,*m[NMSG],*w,*c,*l,*b,*vb,*b0,*lm[NMSG];
  int i;
  char s[SLEN],*p;
  char*tnames[NATREAT]={
  " None",
  " Playfair cipher",
  " Substitution cipher",
  " Fixed Caesar/Vigenère cipher",
  " Variable Caesar cipher (clue order)",
  " Misprint (clue order)",
  " Delete single occurrence of letter (clue order)",
  " Letters latent: delete all occurrences of letter (clue order)",
  " Insert single letter (clue order)",
  " Custom plug-in"};
  char*lab[NATREAT][NMSG]={
    {0,                     0,                    },
    {"Keyword: ",           0,                    },
    {"Encode ABC...Z as: ", 0,                    },
    {"Key letter/word: ",   0,                    },
    {"Encodings of A: ",    0,                    },
    {"Correct letters: ",   "Misprinted letters: "},
    {"Letters to delete: ", 0,                    },
    {"Letters to delete: ", 0,                    },
    {"Letters to insert: ", 0,                    },
    {"Message 1: ",         "Message 2: "         }};

  dia=gtk_dialog_new_with_buttons("Answer treatment",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  c=gtk_combo_box_new_text();
  for(i=0;i<NATREAT;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(c),tnames[i]);

  gtk_box_pack_start(GTK_BOX(vb),c,FALSE,FALSE,0);

    int browse(GtkWidget*w,gpointer p) {
      GtkWidget*dia;
      DEB1 printf("w=%08x p=%08x\n",(int)w,(int)p);fflush(stdout);
      dia=gtk_file_selection_new("Choose a treatment plug-in");
      if(gtk_dialog_run(GTK_DIALOG(dia))==GTK_RESPONSE_OK)
        gtk_entry_set_text(GTK_ENTRY(f),gtk_file_selection_get_filename((GtkFileSelection*)dia));
      gtk_widget_destroy(dia);
      return 1;
    }

    int setactive() {int i,j,k;
      k=gtk_combo_box_get_active(GTK_COMBO_BOX(c));
      for(i=0;i<NMSG;i++) {
        j=lab[k][i]!=NULL;
        gtk_widget_set_sensitive(m[i],j);
        gtk_label_set_text(GTK_LABEL(lm[i]),lab[j?k:(NATREAT-1)][i]);
        }
      gtk_widget_set_sensitive(b0,k==NATREAT-1);
      return 1;
      }

  for(i=0;i<NMSG;i++) {
    m[i]=gtk_hbox_new(0,2);
    lm[i]=gtk_label_new(" ");
    gtk_label_set_width_chars(GTK_LABEL(lm[i]),20);
    gtk_misc_set_alignment(GTK_MISC(lm[i]),1,0.5);
    gtk_box_pack_start(GTK_BOX(m[i]),lm[i],FALSE,FALSE,0);
    e[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e[i]),30);
    gtk_entry_set_max_length(GTK_ENTRY(e[i]),SLEN);
    gtk_entry_set_text(GTK_ENTRY(e[i]),treatmsg[i]);
    gtk_box_pack_start(GTK_BOX(m[i]),e[i],FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),m[i],TRUE,TRUE,0);
    }

  b0=gtk_hbox_new(0,2);
  sprintf(s,"Treatment plug-in: ");
  l=gtk_label_new(s);
  gtk_label_set_width_chars(GTK_LABEL(l),20);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  gtk_box_pack_start(GTK_BOX(b0),l,FALSE,FALSE,0);
  f=gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(f),SLEN);
  gtk_entry_set_text(GTK_ENTRY(f),tpifname);
  gtk_box_pack_start(GTK_BOX(b0),f,FALSE,FALSE,0);
  b=gtk_button_new_with_label("Browse...");
  gtk_box_pack_start(GTK_BOX(b0),b,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(browse),0);
  gtk_box_pack_start(GTK_BOX(vb),b0,TRUE,TRUE,0);
  w=gtk_check_button_new_with_mnemonic("Treated answer _must be a word");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),tambaw);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  gtk_signal_connect(GTK_OBJECT(c),"changed",GTK_SIGNAL_FUNC(setactive),0);
  gtk_combo_box_set_active(GTK_COMBO_BOX(c),treatmode);
  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    strncpy(tpifname,gtk_entry_get_text(GTK_ENTRY(f)),SLEN-1);
    tpifname[SLEN-1]=0;
    treatmode=gtk_combo_box_get_active(GTK_COMBO_BOX(c));
    for(i=0;i<NMSG;i++) {
      strncpy(treatmsg[i],gtk_entry_get_text(GTK_ENTRY(e[i])),SLEN-1);
      treatmsg[i][SLEN-1]=0;
      }
    tambaw=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
    if(treatmode<0||treatmode>NATREAT) treatmode=0;
    if(treatmode==NATREAT-1) {
      if((p=loadtpi())) {
        sprintf(s,"Error loading custom plug-in\n%.100s",p);
        reperr(s);
        }
      }
    else unloadtpi();
    }
  gtk_widget_destroy(dia);
  gridchange();
  return 1;
  }



// dictionary list dialogue
static int dictldia(void) {
  GtkWidget*dia,*e[MAXNDICTS],*l,*b,*t,*f0[MAXNDICTS],*f1[MAXNDICTS];
  int i,j;
  char s[SLEN];

  dia=gtk_dialog_new_with_buttons("Dictionaries",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  t=gtk_table_new(MAXNDICTS+1,5,0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),t,TRUE,TRUE,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>File</b>"         ); gtk_table_attach(GTK_TABLE(t),l,1,2,0,1,0,0,0,0);
  l=gtk_label_new("");                                                             gtk_table_attach(GTK_TABLE(t),l,3,4,0,1,0,0,10,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>File filter</b>"  ); gtk_table_attach(GTK_TABLE(t),l,4,5,0,1,0,0,0,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Answer filter</b>"); gtk_table_attach(GTK_TABLE(t),l,5,6,0,1,0,0,0,0);
  for(i=0;i<MAXNDICTS;i++) {

    int browse(GtkWidget*w,gpointer p) {
      GtkWidget*dia;
      DEB1 printf("w=%08x p=%08x\n",(int)w,(int)p);fflush(stdout);
      dia=gtk_file_selection_new("Choose a dictionary");
      if(gtk_dialog_run(GTK_DIALOG(dia))==GTK_RESPONSE_OK)
        gtk_entry_set_text(GTK_ENTRY(e[(int)p]),gtk_file_selection_get_filename((GtkFileSelection*)dia));
      gtk_widget_destroy(dia);
      return 1;
    }

    sprintf(s,"<b>%d</b>",i+1);
    l=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(l),s);
    gtk_table_attach(GTK_TABLE(t),l,0,1,i+1,i+2,0,0,5,0);
    e[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(e[i]),SLEN);
    gtk_entry_set_text(GTK_ENTRY(e[i]),dfnames[i]);
    gtk_table_attach(GTK_TABLE(t),e[i],1,2,i+1,i+2,0,0,0,0);
    b=gtk_button_new_with_label("Browse...");
    gtk_table_attach(GTK_TABLE(t),b,2,3,i+1,i+2,0,0,0,0);
    f0[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(f0[i]),SLEN);
    gtk_entry_set_text(GTK_ENTRY(f0[i]),dsfilters[i]);
    gtk_table_attach(GTK_TABLE(t),f0[i],4,5,i+1,i+2,0,0,5,0);
    f1[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(f1[i]),SLEN);
    gtk_entry_set_text(GTK_ENTRY(f1[i]),dafilters[i]);
    gtk_table_attach(GTK_TABLE(t),f1[i],5,6,i+1,i+2,0,0,5,0);
    gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(browse),(void*)i);
    }
  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    for(j=0;j<MAXNDICTS;j++) {
      strncpy(dfnames[j],gtk_entry_get_text(GTK_ENTRY(e[j])),SLEN-1);
      dfnames[j][SLEN-1]=0;
      strncpy(dsfilters[j],gtk_entry_get_text(GTK_ENTRY(f0[j])),SLEN-1);
      dsfilters[j][SLEN-1]=0;
      strncpy(dafilters[j],gtk_entry_get_text(GTK_ENTRY(f1[j])),SLEN-1);
      dafilters[j][SLEN-1]=0;
      }
    loaddicts(0);
    }
  gtk_widget_destroy(dia);
  gridchange();
  return 1;
  }


// grid properties dialogue
static int gpropdia(void) {
  GtkWidget*dia,*l,*lx,*ly,*w,*w30,*w31,*w20,*vb,*t,*ttl,*aut;
  int i,m;
  dia=gtk_dialog_new_with_buttons("Grid properties",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  t=gtk_table_new(2,2,0);                                                           gtk_box_pack_start(GTK_BOX(vb),t,TRUE,TRUE,0);
  l=gtk_label_new("Title: ");                                                       gtk_table_attach(GTK_TABLE(t),l,0,1,0,1,0,0,0,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  ttl=gtk_entry_new();                                                              gtk_table_attach(GTK_TABLE(t),ttl,1,2,0,1,0,0,0,0);
  gtk_entry_set_max_length(GTK_ENTRY(ttl),SLEN);
  gtk_entry_set_text(GTK_ENTRY(ttl),gtitle);
  l=gtk_label_new("Author: ");                                                      gtk_table_attach(GTK_TABLE(t),l,0,1,1,2,0,0,0,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  aut=gtk_entry_new();                                                              gtk_table_attach(GTK_TABLE(t),aut,1,2,1,2,0,0,0,0);
  gtk_entry_set_max_length(GTK_ENTRY(aut),SLEN);
  gtk_entry_set_text(GTK_ENTRY(aut),gauthor);


  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("Grid type: ");                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  w20=gtk_combo_box_new_text();                                                     gtk_box_pack_start(GTK_BOX(w),w20,FALSE,FALSE,0);
  for(i=0;i<NGTYPE;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(w20),gtypedesc[i]);

  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("Size: ");                                                        gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  w30=gtk_spin_button_new_with_range(1,MXSZ,1);                                     gtk_box_pack_start(GTK_BOX(w),w30,FALSE,FALSE,0);
  lx=gtk_label_new(" ");                                                            gtk_box_pack_start(GTK_BOX(w),lx,FALSE,FALSE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("by ");                                                           gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  w31=gtk_spin_button_new_with_range(1,MXSZ,1);                                     gtk_box_pack_start(GTK_BOX(w),w31,FALSE,FALSE,0);
  ly=gtk_label_new(" ");                                                            gtk_box_pack_start(GTK_BOX(w),ly,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

    int gtchanged(GtkWidget*w,gpointer p) {
    i=gtk_combo_box_get_active(GTK_COMBO_BOX(w20));
    switch(i) { // gtype
    case 0: case 1: case 2:
      gtk_label_set_text(GTK_LABEL(lx)," columns");
      gtk_label_set_text(GTK_LABEL(ly)," rows");
      break;
    case 3: case 4:
      gtk_label_set_text(GTK_LABEL(lx)," radii");
      gtk_label_set_text(GTK_LABEL(ly)," annuli");
      break;
      }
    return 1;
    }

  gtk_signal_connect(GTK_OBJECT(w20),"changed",GTK_SIGNAL_FUNC(gtchanged),NULL); // for update of labels

  // set widgets from current values
  gtk_combo_box_set_active(GTK_COMBO_BOX(w20),gtype);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w30),width);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w31),height);

  gtk_widget_show_all(dia);

  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    // resync everything
    i=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w30)); if(i>=1&&i<=MXSZ) width=i;
    i=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w31)); if(i>=1&&i<=MXSZ) height=i;
    i=gtk_combo_box_get_active(GTK_COMBO_BOX(w20)); if(i>=0&&i<NGTYPE) gtype=i;
    strncpy(gtitle ,gtk_entry_get_text(GTK_ENTRY(ttl)),SLEN-1); gtitle [SLEN-1]=0;
    strncpy(gauthor,gtk_entry_get_text(GTK_ENTRY(aut)),SLEN-1); gauthor[SLEN-1]=0;
    }

  gtk_widget_destroy(dia);
  draw_init();
  m=symmrmask(); if(((1<<symmr)&m)==0) symmr=1;
  m=symmmmask(); if(((1<<symmm)&m)==0) symmm=0;
  m=symmdmask(); if(((1<<symmd)&m)==0) symmd=0;
  gridchange();
  syncgui();
  return 1;
  }


// preferences dialogue
static int prefsdia(void) {
  GtkWidget*dia,*l,*w,*w30,*w31,*w00,*w01,*w02,*w20,*w21,*w10,*w11,*w12,*w14,*vb;
  int i;
  dia=gtk_dialog_new_with_buttons("Preferences",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Export preferences</b>");         gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("EPS export square size: ");                                                  gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w30=gtk_spin_button_new_with_range(10,72,1);                                                  gtk_box_pack_start(GTK_BOX(w),w30,FALSE,FALSE,0);
  l=gtk_label_new(" points");                                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("HTML/PNG export square size: ");                                             gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w31=gtk_spin_button_new_with_range(10,72,1);                                                  gtk_box_pack_start(GTK_BOX(w),w31,FALSE,FALSE,0);
  l=gtk_label_new(" pixels");                                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);


  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Editing preferences</b>");        gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
  w00=gtk_check_button_new_with_label("Clicking corners makes blocks");                         gtk_box_pack_start(GTK_BOX(vb),w00,TRUE,TRUE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w01=gtk_check_button_new_with_label("Clicking edges makes bars");                             gtk_box_pack_start(GTK_BOX(vb),w01,TRUE,TRUE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w02=gtk_check_button_new_with_label("Show numbers while editing");                            gtk_box_pack_start(GTK_BOX(vb),w02,TRUE,TRUE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Statistics preferences</b>");     gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
  l=gtk_label_new("Desirable checking ratios");gtk_misc_set_alignment(GTK_MISC(l),0,0.5);       gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("Minimum: ");                                                                 gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w20=gtk_spin_button_new_with_range(0,100,1);                                                  gtk_box_pack_start(GTK_BOX(w),w20,FALSE,FALSE,0);
  l=gtk_label_new("%   Maximum: ");                                                             gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w21=gtk_spin_button_new_with_range(0,100,1);                                                  gtk_box_pack_start(GTK_BOX(w),w21,FALSE,FALSE,0);
  l=gtk_label_new("% plus one cell");                                                           gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Autofill preferences</b>");       gtk_box_pack_start(GTK_BOX(vb),l,TRUE,TRUE,0);
  w10=gtk_radio_button_new_with_label_from_widget(NULL,"Deterministic");                        gtk_box_pack_start(GTK_BOX(vb),w10,TRUE,TRUE,0);
  w11=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(w10),"Slightly randomised"); gtk_box_pack_start(GTK_BOX(vb),w11,TRUE,TRUE,0);
  w12=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(w11),"Highly randomised");   gtk_box_pack_start(GTK_BOX(vb),w12,TRUE,TRUE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w14=gtk_check_button_new_with_label("Prevent duplicate answers and lights");                  gtk_box_pack_start(GTK_BOX(vb),w14,TRUE,TRUE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  // set widgets from current preferences values
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w30),eptsq);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w31),hpxsq);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w00),clickblock);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w01),clickbar);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w02),shownums);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w14),afunique);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w20),mincheck);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w21),maxcheck);
  if(afrandom==0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w10),1);
  if(afrandom==1) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w11),1);
  if(afrandom==2) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w12),1);

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    // set preferences values back from values (with bounds checking)
    eptsq=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w30));
    if(eptsq<10) eptsq=10;
    if(eptsq>72) eptsq=72;
    hpxsq=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w31));
    if(hpxsq<10) hpxsq=10;
    if(hpxsq>72) hpxsq=72;
    clickblock=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w00))&1;
    clickbar=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w01))&1;
    shownums=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w02))&1;
    mincheck=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w20));
    if(mincheck<  0) mincheck=  0;
    if(mincheck>100) mincheck=100;
    maxcheck=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w21));
    if(maxcheck<  0) maxcheck=  0;
    if(maxcheck>100) maxcheck=100;
    afunique=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w14));
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w10))) afrandom=0;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w11))) afrandom=1;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w12))) afrandom=2;
    saveprefs(); // save to preferences file (failing silently)
    }
  gtk_widget_destroy(dia);
  compute(); // because ofnew checking ratios
  invaldaall();
  return 1;
  }

// update statistics window if in view
// main widget table is st_te[][], rows below in st_r[]
void stats_upd(void) {
  int i,j;
  char s[SLEN];

  if(!stats) return;
  for(i=2;i<=MXSZ;i++) { // one row for each word length
    if(st_lc[i]>0) {
      sprintf(s,"%d",i);                                                                                      gtk_label_set_text(GTK_LABEL(st_te[i][0]),s);
      sprintf(s,"%d",st_lc[i]);                                                                               gtk_label_set_text(GTK_LABEL(st_te[i][1]),s);
      sprintf(s,st_lucc[i]?"%d (%.1f%%)":" ",st_lucc[i],100.0*st_lucc[i]/st_lc[i]);                           gtk_label_set_text(GTK_LABEL(st_te[i][2]),s);
      sprintf(s,st_locc[i]?"%d (%.1f%%)":" ",st_locc[i],100.0*st_locc[i]/st_lc[i]);                           gtk_label_set_text(GTK_LABEL(st_te[i][3]),s);
      sprintf(s,st_lc[i]?"%.2f:%.2f:%.2f":" "   ,1.0*st_lmnc[i]/i,1.0*st_lsc[i]/st_lc[i]/i,1.0*st_lmxc[i]/i); gtk_label_set_text(GTK_LABEL(st_te[i][4]),s);
      for(j=0;j<5;j++) gtk_widget_show(st_te[i][j]); // show row if non-empty...
      }
    else for(j=0;j<5;j++) {gtk_label_set_text(GTK_LABEL(st_te[i][j])," ");gtk_widget_hide(st_te[i][j]);} // ... and hide row if empty
    }
  sprintf(s,"Total lights: %d",nw);                                                        gtk_label_set_text(GTK_LABEL(st_r[0]),s);
  if(nw>0) sprintf(s,"Mean length: %.1f",(double)nc/nw);
  else strcpy (s,"Mean length: -");                                                        gtk_label_set_text(GTK_LABEL(st_r[1]),s);
  if(nc>0) sprintf(s,"Checked light letters: %d/%d (%.1f%%)",st_sc,nc,100.0*st_sc/nc);
  else     strcpy (s,"Checked light letters: -");                                          gtk_label_set_text(GTK_LABEL(st_r[2]),s);
  if(ne>0) sprintf(s,"Checked grid cells: %d/%d (%.1f%%)",st_ce,ne,100.0*st_ce/ne);
  else     strcpy (s,"Checked grid cells: -");                                             gtk_label_set_text(GTK_LABEL(st_r[3]),s);
  sprintf(s,"Lights with double unches: %d",st_2u-st_3u);                                  gtk_label_set_text(GTK_LABEL(st_r[4]),s);
  sprintf(s,"Lights with triple, quadruple etc. unches: %d",st_3u);                        gtk_label_set_text(GTK_LABEL(st_r[5]),s);
  }

// create statistics dialogue
static void stats_init(void) {
  int i,j;
  GtkWidget*w0,*w1,*vb;

  for(i=0;i<MXSZ+2;i++) for(j=0;j<5;j++) st_te[i][j]=0;
  stats=gtk_dialog_new_with_buttons("Statistics",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CLOSE,GTK_RESPONSE_ACCEPT,NULL);
  gtk_window_set_policy(GTK_WINDOW(stats),FALSE,FALSE,TRUE);
  vb=gtk_vbox_new(0,3); // main box for widgets
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(stats)->vbox),vb,FALSE,TRUE,0);
  w0=gtk_table_new(MXSZ+2,5,FALSE);
  st_te[0][0]=gtk_label_new("  Length  ");
  st_te[0][1]=gtk_label_new("  Count  ");
  st_te[0][2]=gtk_label_new("  Underchecked  ");
  st_te[0][3]=gtk_label_new("  Overchecked  ");
  st_te[0][4]=gtk_label_new("  Check ratio min:mean:max  ");
  for(j=0;j<5;j++) gtk_widget_show(st_te[0][j]);
  w1=gtk_hseparator_new();gtk_table_attach_defaults(GTK_TABLE(w0),w1,0,5,1,2);gtk_widget_show(w1);
  for(i=2;i<=MXSZ;i++) for(j=0;j<5;j++) st_te[i][j]=gtk_label_new(""); // initialise all table entries to empty strings
  for(i=0;i<=MXSZ;i++) for(j=0;j<5;j++) if(st_te[i][j]) gtk_table_attach_defaults(GTK_TABLE(w0),st_te[i][j],j,j+1,i,i+1);
  w1=gtk_hseparator_new();gtk_table_attach_defaults(GTK_TABLE(w0),w1,0,5,MXSZ+1,MXSZ+2);gtk_widget_show(w1);
  gtk_box_pack_start(GTK_BOX(vb),w0,FALSE,TRUE,0);gtk_widget_show(w0);

  for(j=0;j<6;j++) {
    st_r[j]=gtk_label_new(" "); // blank rows at the bottom for now
    gtk_misc_set_alignment(GTK_MISC(st_r[j]),0,0.5);
    gtk_box_pack_start(GTK_BOX(vb),st_r[j],FALSE,TRUE,0);
    gtk_widget_show(st_r[j]);
    }
  gtk_widget_show(vb);
  }

// remove statistics window (if not already gone or in the process of going)
void stats_quit(GtkDialog*w,int i0,void*p0) {
  DEB4 printf("stats_quit()\n"),fflush(stdout);
  if(i0!=GTK_RESPONSE_DELETE_EVENT&&stats!=NULL) gtk_widget_destroy(stats);
  stats=NULL;
  DEB4 printf("stats_quit()\n"),fflush(stdout);
  }

// open stats window if not already open, and update it
static int statsdia(void) {
  if(!stats) stats_init();
  stats_upd();
  gtk_widget_show(stats);
  gtk_signal_connect(GTK_OBJECT(stats),"response",GTK_SIGNAL_FUNC(stats_quit),NULL); // does repeating this matter?
  return 0;
  }

// general question-and-answer box: title, question, yes-text, no-text
static int box(int type,const char*t,const char*u,const char*v) {
  GtkWidget*dia;
  int i;

  dia=gtk_message_dialog_new(
    GTK_WINDOW(mainw),
    GTK_DIALOG_DESTROY_WITH_PARENT,
    type,
    GTK_BUTTONS_NONE,
    "%s",t
    );
  gtk_dialog_add_buttons(GTK_DIALOG(dia),u,GTK_RESPONSE_ACCEPT,v,GTK_RESPONSE_REJECT,NULL);
  currentdia=dia;
  i=gtk_dialog_run(GTK_DIALOG(dia));
  currentdia=0;
  gtk_widget_destroy(dia);
  return i==GTK_RESPONSE_ACCEPT;
  }

void killcurrdia(void) {
  if(currentdia) gtk_dialog_response(GTK_DIALOG(currentdia),GTK_RESPONSE_CANCEL); // kill the current dialogue box
  }

void setposslabel(char*s) {
  gtk_label_set_text(GTK_LABEL(poss_label),s);
  }

// update feasible list to screen
void updatefeas(void) {int i; char t0[MXSZ+100],t1[MXSZ+50],*u[2],p0[SLEN],p1[SLEN];struct answer*a;
  u[0]=t0;
  u[1]=t1;
  p1[0]='\0';
  gtk_clist_freeze(GTK_CLIST(clist)); // avoid glitchy-looking updates
  gtk_clist_clear(GTK_CLIST(clist));
  if(!isclear(curx,cury)) goto ew0;
  for(i=0;i<llistn;i++) { // add in list entries
    t0[0]=0;
    a=ansp[lts[llistp[i]].ans];
    for(;;) {
      if(a->cfdmask&llistdm) {
        if(strlen(t0)+strlen(a->cf)>MXSZ+90) {strcat(t0,"...");break;}
        strcat(t0,a->cf);
        if(a->acf) strcat(t0,", ");
        }
      if(!a->acf) break;
      a=a->acf;
      }
    strcat(t0," ");
    strcat(t0,lemdesc[lts[llistp[i]].em]);
    sprintf(t1,"%+.1f",log10(ansp[lts[llistp[i]].ans]->score));
    gtk_clist_append(GTK_CLIST(clist),u);
    }
  if(gsq[curx][cury].ch==' ') {
    getposs(gsq[curx][cury].e,p0,0); // get feasible letter list
    if(gsq[curx][cury].flbm==0||strlen(p0)==0) sprintf(p1," No feasible letters");
    else                                       sprintf(p1," Feasible letter%s: %s",(strlen(p0)==1)?"":"s",p0);
  } else p1[0]=0;
  ew0:
  gtk_clist_thaw(GTK_CLIST(clist)); // make list visible
  DEB1 printf("Setting poss label to >%s<\n",p1);
  setposslabel(p1);
  return;
  }

// box for information purposes only
void okbox(const char*t) {box(GTK_MESSAGE_INFO,t,GTK_STOCK_CLOSE,0);}

// box for errors, with `details' option
void reperr(const char*s) {box(GTK_MESSAGE_ERROR,s,GTK_STOCK_CLOSE,0);}
void oomerr() {box(GTK_MESSAGE_ERROR,"Out of memory",GTK_STOCK_CLOSE,0);}
void fnferr() {box(GTK_MESSAGE_ERROR,"File not found",GTK_STOCK_CLOSE,0);}
void fsgerr() {box(GTK_MESSAGE_ERROR,"Filing system error",GTK_STOCK_CLOSE,0);}
void fserror() {char s[SLEN],t[SLEN*2];
  if(strerror_r(errno,s,SLEN)) strcpy(s,"general error");
  sprintf(t,"Filing system error: %s",s);
  reperr(t);
  }

int areyousure(void) { // general-purpose are-you-sure dialogue
  return box(GTK_MESSAGE_QUESTION,"\n  Your work is not saved.  \n  Are you sure you want to proceed?  \n","  Proceed  ",GTK_STOCK_CANCEL);
  }

// sync GUI with possibly new grid properties, flags, symmetry, width, height
void syncgui(void) {int i,m;
  if(!isingrid(curx,cury)) {curx=0,cury=0;} // keep cursor in grid
  if(dir>=ndir[gtype]) dir=0; // make sure direction is feasible
  gtk_drawing_area_size(GTK_DRAWING_AREA(grid_da),dawidth()+3,daheight()+3);
  gtk_widget_show(grid_da);
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x100+symmr));
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x200+symmm));
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x300+symmd));
  m=symmrmask(); for(i=1;i<=12;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x100+i),(m>>i)&1);
  m=symmmmask(); for(i=0;i<= 3;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x200+i),(m>>i)&1);
  m=symmdmask(); for(i=0;i<= 3;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x300+i),(m>>i)&1);
  i=(gtype==3||gtype==4);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf000),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf001), i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf002), i);
  i=(gtype>0);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x403),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x413),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x433),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x443),!i);
  setwintitle();
  draw_init();
  refreshall(0,shownums?7:3);
  curmoved();
  }

// menus
static GtkItemFactoryEntry menu_items[] = {
 { "/_File",                                          0,                   0,              0,     "<Branch>"},
 { "/File/_New",                                      "<control>N",        m_filenew,      0,     "<StockItem>",GTK_STOCK_NEW},
 { "/File/sep0",                                      0,                   0,              0,     "<Separator>"},
 { "/File/_Open...",                                  "<control>O",        m_fileopen,     0,     "<StockItem>",GTK_STOCK_OPEN},
 { "/File/_Save as...",                               "<control>S",        m_filesave,     0,     "<StockItem>",GTK_STOCK_SAVE_AS},
 { "/File/sep1",                                      0,                   0,              0,     "<Separator>"},
 { "/File/Export _blank grid image",                  0,                   0,              0,     "<Branch>"},
 { "/File/Export blank grid image/as _EPS...",        0,                   m_fileexport,   0x401},
 { "/File/Export blank grid image/as _PNG...",        0,                   m_fileexport,   0x402},
 { "/File/Export blank grid image/as _HTML...",       0,                   m_fileexport,   0x403},
 { "/File/Export _filled grid image",                 0,                   0,              0,     "<Branch>"},
 { "/File/Export filled grid image/as _EPS...",       0,                   m_fileexport,   0x411},
 { "/File/Export filled grid image/as _PNG...",       0,                   m_fileexport,   0x412},
 { "/File/Export filled grid image/as _HTML...",      0,                   m_fileexport,   0x413},
 { "/File/Export _answers",                           0,                   0,              0,     "<Branch>"},
 { "/File/Export answers/As _text...",                0,                   m_fileexport,   0x420},
 { "/File/Export answers/As _HTML...",                0,                   m_fileexport,   0x423},
 { "/File/Export _puzzle",                            0,                   0,              0,     "<Branch>"},
 { "/File/Export puzzle/As _HTML...",                 0,                   m_fileexport,   0x433},
 { "/File/Export puzzle/As HTML+_PNG...",             0,                   m_fileexport,   0x434},
 { "/File/Export so_lution",                          0,                   0,              0,     "<Branch>"},
 { "/File/Export solution/As _HTML...",               0,                   m_fileexport,   0x443},
 { "/File/Export solution/As HTML+_PNG...",           0,                   m_fileexport,   0x444},
 { "/File/sep2",                                      0,                   0,              0,     "<Separator>"},
 { "/File/_Quit",                                     "<control>Q",        m_filequit,     0,     "<StockItem>",GTK_STOCK_QUIT},
 { "/_Edit",                                          0,                   0,              0,     "<Branch>"},
 { "/Edit/_Undo",                                     "<control>Z",        m_undo,         0,     "<StockItem>",GTK_STOCK_UNDO},
 { "/Edit/_Redo",                                     "<control>Y",        m_redo,         0,     "<StockItem>",GTK_STOCK_REDO},
 { "/Edit/sep1",                                      0,                   0,              0,     "<Separator>"},
 { "/Edit/_Solid block",                              "Insert",            m_editblock},
 { "/Edit/_Bar before",                               "Return",            m_editbarb},
 { "/Edit/_Empty",                                    "Delete",            m_editspace},
 { "/Edit/_Cutout",                                   "<control>C",        m_editcutout},
 { "/Edit/_Merge with next",                          "<control>M",        m_editmerge},
 { "/Edit/sep2",                                      0,                   0,              0,     "<Separator>"},
 { "/Edit/C_lear selected cells",                     "<shift><control>X",        m_erase,        0,     "<StockItem>",GTK_STOCK_CLEAR},
 { "/Edit/sep3",                                      0,                   0,              0,     "<Separator>"},
 { "/Edit/_Flip in main diagonal",                    0,                   m_editflip,     0xf000},
 { "/Edit/Rotate cloc_kwise",                         "greater",           m_editrot,      0xf001},
 { "/Edit/Rotate _anticlockwise",                     "less",              m_editrot,      0xf002},
 { "/Edit/sep4",                                      0,                   0,              0,     "<Separator>"},
 { "/Edit/_Zoom",                                     0,                   0,              0,     "<Branch>"},
 { "/Edit/Zoom/_Out",                                 "<control>minus",    m_zoom,         -2},
 { "/Edit/Zoom/_1 50%",                               "<control>8",        m_zoom,         0},
 { "/Edit/Zoom/_2 71%",                               "<control>9",        m_zoom,         1},
 { "/Edit/Zoom/_3 100%",                              "<control>0",        m_zoom,         2},
 { "/Edit/Zoom/_4 141%",                              "<control>1",        m_zoom,         3},
 { "/Edit/Zoom/_5 200%",                              "<control>2",        m_zoom,         4},
 { "/Edit/Zoom/_In",                                  "<control>plus",     m_zoom,         -1},
 { "/Edit/Show s_tatistics",                          0,                   m_showstats},
 { "/Edit/_Preferences...",                           0,                   m_editprefs,    0,     "<StockItem>", GTK_STOCK_PREFERENCES},
 { "/_Properties",                                    0,                   0,              0,     "<Branch>"},
 { "/Properties/_Grid properties...",                 0,                   m_editgprop,    0,     "<StockItem>",GTK_STOCK_PROPERTIES},
 { "/Properties/Default _cell properties...",         0,                   m_dsprop,       0},
 { "/Properties/Selected c_ell properties...",        0,                   m_sprop,        0},
 { "/Properties/Default _light properties...",        0,                   m_dlprop,       0},
 { "/Properties/Selected l_ight properties...",       0,                   m_lprop,        0},
 { "/_Select",                                        0,                   0,              0,     "<Branch>"},
 { "/Select/Current _cell",                           "<shift>C",          m_selcell},
 { "/Select/Current _light",                          "<shift>L",          m_sellight},
 { "/Select/Cell _mode <> light mode",                "<shift>M",          m_selmode},
 { "/Select/sep0",                                    0,                   0,              0,     "<Separator>"},
 { "/Select/_All",                                    "<shift>A",          m_selall},
 { "/Select/_Invert",                                 "<shift>I",          m_selinv},
 { "/Select/_Nothing",                                "<shift>N",          m_selnone},
 { "/Select/sep1",                                    0,                   0,              0,     "<Separator>"},
 { "/Select/Cell_s",                                  0,                   0,              0,     "<Branch>"},
 { "/Select/Cells/overriding default _properties",    0,                   m_selcover},
// { "/Select/Cells/with answer treatment _enabled",    0,                   m_selctreat},
 { "/Select/Li_ghts",                                 0,                   0,              0,     "<Branch>"},
 { "/Select/Lights/_in current direction",            0,                   m_sellpar},
 { "/Select/Lights/overriding default _properties",   0,                   m_sellover},
 { "/Select/Lights/with answer treatment _enabled",   0,                   m_selltreat},
 { "/Select/Lights/with _double or more unches",      0,                   m_selviol,      1},
 { "/Select/Lights/with _triple or more unches",      0,                   m_selviol,      2},
 { "/Select/Lights/that are _underchecked",           0,                   m_selviol,      4},
 { "/Select/Lights/that are _overchecked",            0,                   m_selviol,      8},
 { "/Sy_mmetry",                                      0,                   0,              0,     "<Branch>"},
 { "/Symmetry/No rotational",                         0,                   m_symm0,        0x0101,"<RadioItem>"},
 { "/Symmetry/Twofold rotational",                    0,                   m_symm0,        0x0102,"/Symmetry/No rotational"         },
 { "/Symmetry/Threefold rotational",                  0,                   m_symm0,        0x0103,"/Symmetry/Twofold rotational"    },
 { "/Symmetry/Fourfold rotational",                   0,                   m_symm0,        0x0104,"/Symmetry/Threefold rotational"  },
 { "/Symmetry/Fivefold rotational",                   0,                   m_symm0,        0x0105,"/Symmetry/Fourfold rotational"   },
 { "/Symmetry/Sixfold rotational",                    0,                   m_symm0,        0x0106,"/Symmetry/Fivefold rotational"   },
 { "/Symmetry/Sevenfold rotational",                  0,                   m_symm0,        0x0107,"/Symmetry/Sixfold rotational"    },
 { "/Symmetry/Eightfold rotational",                  0,                   m_symm0,        0x0108,"/Symmetry/Sevenfold rotational"  },
 { "/Symmetry/Ninefold rotational",                   0,                   m_symm0,        0x0109,"/Symmetry/Eightfold rotational"  },
 { "/Symmetry/Tenfold rotational",                    0,                   m_symm0,        0x010a,"/Symmetry/Ninefold rotational"   },
 { "/Symmetry/Elevenfold rotational",                 0,                   m_symm0,        0x010b,"/Symmetry/Tenfold rotational"    },
 { "/Symmetry/Twelvefold rotational",                 0,                   m_symm0,        0x010c,"/Symmetry/Elevenfold rotational" },
 { "/Symmetry/sep1",                                  0,                   0,              0,     "<Separator>"},
 { "/Symmetry/No mirror",                             0,                   m_symm1,        0x0200,"<RadioItem>"},
 { "/Symmetry/Left-right mirror",                     0,                   m_symm1,        0x0201,"/Symmetry/No mirror"},
 { "/Symmetry/Up-down mirror",                        0,                   m_symm1,        0x0202,"/Symmetry/Left-right mirror"},
 { "/Symmetry/Both",                                  0,                   m_symm1,        0x0203,"/Symmetry/Up-down mirror"},
 { "/Symmetry/sep2",                                  0,                   0,              0,     "<Separator>"},
 { "/Symmetry/No duplication",                        0,                   m_symm2,        0x0300,"<RadioItem>"},
 { "/Symmetry/Left-right duplication",                0,                   m_symm2,        0x0301,"/Symmetry/No duplication"},
 { "/Symmetry/Up-down duplication",                   0,                   m_symm2,        0x0302,"/Symmetry/Left-right duplication"},
 { "/Symmetry/Both",                                  0,                   m_symm2,        0x0303,"/Symmetry/Up-down duplication"},
 { "/_Autofill",                                      0,                   0,              0,     "<Branch>"},
 { "/Autofill/_Dictionaries...",                      0,                   m_dictionaries},
 { "/Autofill/Answer _treatment...",                  0,                   m_afctreat},
 { "/Autofill/sep1",                                  0,                   0,              0,     "<Separator>"},
 { "/Autofill/Auto_fill",                             "<control>G",        m_autofill,     1,     "<StockItem>",GTK_STOCK_EXECUTE},
 { "/Autofill/Autofill _selected cells",              "<shift><control>G", m_autofill,     2,     "<StockItem>",GTK_STOCK_EXECUTE},
 { "/Autofill/Accept _hints",                         "<control>A",        m_accept},
 { "/Help",                                           0,                   0,              0,     "<LastBranch>"},
 { "/Help/About",                                     0,                   m_helpabout,    0,     "<StockItem>",GTK_STOCK_ABOUT},
};

// build main window and other initialisation
void startgtk(void) {
  GtkAccelGroup*accel_group;

  pxsq=zoompx[zoomf];

  mainw=gtk_window_new(GTK_WINDOW_TOPLEVEL); // main window
  gtk_widget_set_name(mainw,"Qxw");
  gtk_window_set_default_size(GTK_WINDOW(mainw),780,560);
  gtk_window_set_title(GTK_WINDOW(mainw),"Qxw");
  gtk_window_set_position(GTK_WINDOW(mainw),GTK_WIN_POS_CENTER);

  // box in the window
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(mainw),vbox);

  // menu in the vbox
  accel_group=gtk_accel_group_new();
  item_factory=gtk_item_factory_new(GTK_TYPE_MENU_BAR,"<main>",accel_group);
  gtk_item_factory_create_items(item_factory,sizeof(menu_items)/sizeof(menu_items[0]),menu_items,NULL);
  gtk_window_add_accel_group(GTK_WINDOW(mainw),accel_group);
  menubar=gtk_item_factory_get_widget(item_factory,"<main>");
  gtk_box_pack_start(GTK_BOX(vbox),menubar,FALSE,TRUE,0);

  // window is divided into two parts, or `panes'
  paned=gtk_hpaned_new();
  gtk_box_pack_start(GTK_BOX(vbox),paned,TRUE,TRUE,0);

  // scrolled windows in the panes
  grid_sw=gtk_scrolled_window_new(NULL,NULL);
  gtk_container_set_border_width(GTK_CONTAINER(grid_sw),10);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(grid_sw),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_paned_pack1(GTK_PANED(paned),grid_sw,1,1);

  list_sw=gtk_scrolled_window_new(NULL,NULL);
  gtk_container_set_border_width(GTK_CONTAINER(list_sw),10);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_sw),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_paned_pack2(GTK_PANED(paned),list_sw,0,1);
  gtk_paned_set_position(GTK_PANED(paned),560);

  // drawing area for grid and events it captures
  grid_da=gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(grid_da),100,100);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(grid_sw),grid_da);
  GTK_WIDGET_SET_FLAGS(grid_da,GTK_CAN_FOCUS);
  gtk_widget_set_events(grid_da,GDK_EXPOSURE_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_KEY_PRESS_MASK|GDK_POINTER_MOTION_MASK);

  // list of feasible words
//  clist=gtk_clist_new(2);
  clist=gtk_clist_new(1);
  gtk_clist_set_column_width(GTK_CLIST(clist),0,180);
//   gtk_clist_set_column_width(GTK_CLIST(clist),1,80);
  gtk_clist_set_column_title(GTK_CLIST(clist),0,"Feasible words");
//  gtk_clist_set_column_title(GTK_CLIST(clist),1,"Scores");
  gtk_clist_column_titles_passive(GTK_CLIST(clist));
  gtk_clist_column_titles_show(GTK_CLIST(clist));
  gtk_clist_set_selection_mode(GTK_CLIST(clist),GTK_SELECTION_SINGLE);
  gtk_container_add(GTK_CONTAINER(list_sw),clist);

  // box for widgets across the bottom of the window
  hbox=gtk_hbox_new(FALSE,0);
  poss_label=gtk_label_new(" Feasible letters:");
  gtk_box_pack_start(GTK_BOX(hbox),poss_label,FALSE,FALSE,0);
  gtk_box_pack_end(GTK_BOX(vbox),hbox,FALSE,FALSE,0);

  gtk_signal_connect(GTK_OBJECT(grid_da),"expose_event",GTK_SIGNAL_FUNC(expose_event),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"configure_event",GTK_SIGNAL_FUNC(configure_event),NULL);
  gtk_signal_connect(GTK_OBJECT(clist),"select_row",GTK_SIGNAL_FUNC(selrow),NULL);

  gtk_signal_connect(GTK_OBJECT(grid_da),"button_press_event",GTK_SIGNAL_FUNC(button_press_event),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"button_release_event",GTK_SIGNAL_FUNC(button_press_event),NULL);
  gtk_signal_connect_after(GTK_OBJECT(grid_da),"key_press_event",GTK_SIGNAL_FUNC(keypress),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"motion_notify_event",GTK_SIGNAL_FUNC(mousemove),NULL);

  gtk_signal_connect(GTK_OBJECT(mainw),"delete_event",GTK_SIGNAL_FUNC(w_delete),NULL);
  gtk_signal_connect(GTK_OBJECT(mainw),"destroy",GTK_SIGNAL_FUNC(w_destroy),NULL);

  gtk_widget_show_all(mainw);
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  }

void stopgtk(void) {
  }


