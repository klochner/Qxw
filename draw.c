// $Id: draw.c -1   $

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

#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <cairo-ps.h>

#include "common.h"
#include "qxw.h"
#include "draw.h"
#include "gui.h"

int bawdpx,hbawdpx;

static int sfcxoff=0,sfcyoff=0;


double sdirdx[NGTYPE][MAXNDIR]={{1,0},{ 1.2,1.2,0  },{1.4,0.7,-0.7},{1,0},{1,0}};
double sdirdy[NGTYPE][MAXNDIR]={{0,1},{-0.7,0.7,1.4},{0  ,1.2, 1.2},{0,1},{0,1}};
char*dname[NGTYPE][MAXNDIR]={{"Across","Down"},{"Northeast","Southeast","South"},{"East","Southeast","Southwest"},{"Ring","Radial"},{"Ring","Radial"}};












// GRID DRAWING

static void moveto(cairo_t*cc,float x,float y)               {cairo_move_to(cc,x,y);}
static void lineto(cairo_t*cc,float x,float y)               {cairo_line_to(cc,x,y);}
static void rmoveto(cairo_t*cc,float x,float y)              {cairo_rel_move_to(cc,x,y);}
static void rlineto(cairo_t*cc,float x,float y)              {cairo_rel_line_to(cc,x,y);}
static void setlinewidth(cairo_t*cc,float w)                 {cairo_set_line_width(cc,w);}
static void setlinecap(cairo_t*cc,int c)                     {cairo_set_line_cap(cc,c);}
static void closepath(cairo_t*cc)                            {cairo_close_path(cc);}
static void fill(cairo_t*cc)                                 {cairo_fill(cc);}
static void stroke(cairo_t*cc)                               {cairo_stroke(cc);}
static void strokepreserve(cairo_t*cc)                       {cairo_stroke_preserve(cc);}
// static void clip(cairo_t*cc)                                 {cairo_clip(cc);}
static void gsave(cairo_t*cc)                                {cairo_save(cc);}
static void grestore(cairo_t*cc)                             {cairo_restore(cc);}
static void setrgbcolor(cairo_t*cc,float r,float g, float b) {cairo_set_source_rgb(cc,r,g,b);}
static void setfontsize(cairo_t*cc,float h)                  {cairo_set_font_size(cc,h);}
static void showtext(cairo_t*cc,char*s)                      {cairo_show_text(cc,s);}

static float textwidth(cairo_t*cc,char*s,float h) {
  cairo_text_extents_t te;
  cairo_set_font_size(cc,h);
  cairo_text_extents(cc,s,&te);
  return te.x_advance;
  }

// Cairo's arc drawing does not seem to work well as part of a path, so we do our own
static void arc(cairo_t*cc,float x,float y,float r,float t0,float t1) {float u;
  if(t1<t0) t1+=2*PI;
  if((t1-t0)*(t1-t0)*r>1.0/pxsq) { // error of approx 1/4 px?
    u=(t0+t1)/2;
    arc(cc,x,y,r,t0,u);
    arc(cc,x,y,r,   u,t1);
    }
  else lineto(cc,x+r*cos(t1),y+r*sin(t1));
  }

static void arcn(cairo_t*cc,float x,float y,float r,float t0,float t1) {float u;
  if(t0<t1) t0+=2*PI;
  if((t1-t0)*(t1-t0)*r>1.0/pxsq) { // error of approx 1/4 px?
    u=(t0+t1)/2;
    arcn(cc,x,y,r,t0,u);
    arcn(cc,x,y,r,   u,t1);
    }
  else lineto(cc,x+r*cos(t1),y+r*sin(t1));
  }

static void setrgbcolor24(cairo_t*cc,int c) {setrgbcolor(cc,((c>>16)&255)/255.0,((c>>8)&255)/255.0,(c&255)/255.0);}

static void ltext(cairo_t*cc,char*s,float h) {setfontsize(cc,h); showtext(cc,s); }
static void ctext(cairo_t*cc,char*s,float h) {rmoveto(cc,-textwidth(cc,s,h)/2.0,0); setfontsize(cc,h); showtext(cc,s); }






// GRID GEOMETRY

static void vxcoords(float*x0,float*y0,float x,float y,int d) {float t;
  switch(gtype) {
  case 0:
    *x0=x;
    *y0=y;
    switch(d) {
    case 3:                  break;
    case 0:*x0+=1.0;         break;
    case 1:*x0+=1.0;*y0+=1.0;break;
    case 2:         *y0+=1.0;break;
    default:assert(0);break;
      }
    break;
  case 1:
    *x0=x*1.2+.4;
    *y0=y*1.4+((int)floor(x)&1)*.7;
    switch(d) {
    case 5:                  break;
    case 0:*x0+=0.8;         break;
    case 1:*x0+=1.2;*y0+=0.7;break;
    case 2:*x0+=0.8;*y0+=1.4;break;
    case 3:         *y0+=1.4;break;
    case 4:*x0-=0.4;*y0+=0.7;break;
    default:assert(0);break;
      }
    break;
  case 2:
    *y0=y*1.2+.4;
    *x0=x*1.4+((int)floor(y)&1)*.7;
    switch(d) {
    case 4:                  break;
    case 3:*y0+=0.8;         break;
    case 2:*y0+=1.2;*x0+=0.7;break;
    case 1:*y0+=0.8;*x0+=1.4;break;
    case 0:         *x0+=1.4;break;
    case 5:*y0-=0.4;*x0+=0.7;break;
    default:assert(0);break;
      }
    break;
  case 3:case 4:
    switch(d) {
    case 3:              break;
    case 0:x+=1.0;       break;
    case 1:x+=1.0;y+=1.0;break;
    case 2:       y+=1.0;break;
    }
    t=2*PI*((gtype==4)?x-0.5:x)/width;
    *x0= (height-y)*sin(t)+height;
    *y0=-(height-y)*cos(t)+height;
    break;
    }
  }

void edgecoords(float*x0,float*y0,float*x1,float*y1,int x,int y,int d) {
  vxcoords(x0,y0,x,y,d);
  vxcoords(x1,y1,x,y,(d+1)%(ndir[gtype]*2));
  }

// find centre coordinates for merge group containing x,y
void mgcentre(float*u,float*v,int x,int y,int d,int l) {
  float x0=0,y0=0,x1=0,y1=0;
  if(gtype==3||gtype==4) {
    if(EVEN(d)) vxcoords(u,v,x+l/2.0,y+0.5,3);
    else        vxcoords(u,v,x+0.5,y+l/2.0,3);
    }
  else {
    vxcoords(&x0,&y0,x,y,0);
    vxcoords(&x1,&y1,x,y,ndir[gtype]);
    *u=(x0+x1)/2+sdirdx[gtype][d]*(l-1)/2.0;
    *v=(y0+y1)/2+sdirdy[gtype][d]*(l-1)/2.0;
    }
  }

void addvxcoordsbbox(float*x0,float*y0,float*x1,float*y1,float x,float y,int d) {float vx,vy;
  vxcoords(&vx,&vy,x,y,d);
  if(vx<*x0) *x0=vx;
  if(vx>*x1) *x1=vx;
  if(vy<*y0) *y0=vy;
  if(vy>*y1) *y1=vy;
  }

void addsqbbox(float*x0,float*y0,float*x1,float*y1,int x,int y) {int d; float vx,vy;
  mgcentre(&vx,&vy,x,y,0,1);
  if(vx-0.5<*x0) *x0=vx-0.5; // include a 1x1 square centred in the middle of the merge group for the letter
  if(vx+0.5>*x1) *x1=vx+0.5;
  if(vy-0.5<*y0) *y0=vy-0.5;
  if(vy+0.5>*y1) *y1=vy+0.5;
  for(d=0;d<ndir[gtype]*2;d++) addvxcoordsbbox(x0,y0,x1,y1,x,y,d);
  if(gtype!=3&&gtype!=4) return; // done if grid is not circular
  // edge 3 is convex; one of the following points may be tangent to a vertical
  // or horizontal
  addvxcoordsbbox(x0,y0,x1,y1,x+0.25,y,3);
  addvxcoordsbbox(x0,y0,x1,y1,x+0.5 ,y,3);
  addvxcoordsbbox(x0,y0,x1,y1,x+0.75,y,3);
  }

void getsqbbox(float*x0,float*y0,float*x1,float*y1,int x,int y) {
  *x0=1e9; *x1=-1e9;
  *y0=1e9; *y1=-1e9;
  addsqbbox(x0,y0,x1,y1,x,y);
  }

void getmgbbox(float*x0,float*y0,float*x1,float*y1,int x,int y) {int i,l,mx[MXSZ],my[MXSZ];
  *x0=1e9; *x1=-1e9;
  *y0=1e9; *y1=-1e9;
  l=getmergegroup(mx,my,x,y);
  for(i=0;i<l;i++) addsqbbox(x0,y0,x1,y1,mx[i],my[i]);
  }


static void getdxdy(float*c,float*s,int x,int y,int d) {float t,u,v;
  switch(gtype) {
  case 0:case 1:case 2:
    u=sdirdx[gtype][d];
    v=sdirdy[gtype][d];
    t=sqrt(u*u+v*v);
    *c=u/t;*s=v/t;
    break;
  case 3:case 4:
    t=2*PI*((gtype==4)?x:x+0.5)/width;
    if(d==0) {*c= cos(t); *s=sin(t);}
    else     {*c=-sin(t); *s=cos(t);}
    break;
    }
  }

// get suitable scale factor (so that chars fit in small cells) for group
// in direction d of length l
static float getscale(int x,int y,int d,int l) {float s;
  if(gtype<3) return 1;
  s=(height-y-.3)*2*PI/width;
  if(EVEN(d)) s*=l;
  if(s>1) s=1;
  return s;
  }


static void movetoorig(cairo_t*cc,int x,int y) {float x0=0,y0=0;
  switch(gtype) {
  case 0: vxcoords(&x0,&y0,x,y,3);break;
  case 1: vxcoords(&x0,&y0,x,y,5);break;
  case 2: vxcoords(&x0,&y0,x,y,4);break;
  case 3:case 4:
    vxcoords(&x0,&y0,x,y,1);
    break;
    }
  moveto(cc,x0,y0);
  }

static void movetomgcentre(cairo_t*cc,int x,int y,int d,int l) {float u=0,v=0;
  mgcentre(&u,&v,x,y,d,l);
  moveto(cc,u,v);
  }


// draw path for edges with bits set in mask b
static void edges(cairo_t*cc,int x,int y,int b) {int f,g,i,j,n,o;float x0=0,y0=0,x1=0,y1=0,t;
  n=ndir[gtype]*2;
  b&=(1<<n)-1;
  if(b==0) return;
  for(o=0;o<n;o++) if(((b>>o)&1)==0) break; // find a place to start drawing
  f=0;g=0;
  for(i=0;i<ndir[gtype]*2;i++) {
    j=(o+i)%n;
    if((b>>j)&1) {
      edgecoords(&x0,&y0,&x1,&y1,x,y,j);
      if(f==0) moveto(cc,x0,y0);
      if((gtype==3||gtype==4)&&(j&1)) {
        t=2*PI*((gtype==4)?x-0.5:x)/width-PI/2;
        if(j==1) arcn(cc,height,height,height-y-.999,  t+2*PI/width,t);
        else     arc (cc,height,height,height-y     ,t,t+2*PI/width);
        }
      else {
        lineto(cc,x1,y1);
        }
      f=1;
      }
    else f=0,g=1;
    }
  if(g==0) closepath(cc);
  }

// draw path for outline of a `square'
static void sqpath(cairo_t*cc,int x,int y) {int i;float x0=0,y0=0,t;
  vxcoords(&x0,&y0,x,y,0);
  moveto(cc,x0,y0);
  switch(gtype) {
  case 0: case 1: case 2:
    for(i=1;i<ndir[gtype]*2;i++) {
      vxcoords(&x0,&y0,x,y,i);
      lineto(cc,x0,y0);
      }
    break;
  case 3: case 4:
    vxcoords(&x0,&y0,x,y,1);
    lineto(cc,x0,y0);
    t=2*PI*((gtype==4)?x-0.5:x)/width-PI/2;
    arcn(cc,height,height,height-y-.999,t+2*PI/width,t);
    vxcoords(&x0,&y0,x,y,3);
    lineto(cc,x0,y0);
    arc (cc,height,height,height-y     ,t,t+2*PI/width);
    break;
    }
  closepath(cc);
  }

static void rtop(float*r,float*t,float x,float y) {
  x-=height;y=height-y;
  *r=sqrt(x*x+y*y);
  if(*r<1e-3) *t=0;
  else *t=atan2(x,y);
  if(*t<0) *t+=2*PI;
  }

static void ptor(float*x,float*y,float r,float t) {
  *x=r*sin(t)+height;
  *y=height-r*cos(t);
  }

// move to suitable position to plot number of width w, height h
static void movetonum(cairo_t*cc,int x,int y,float w,float h) {float x0=0,y0=0,r0,t0;
  switch(gtype) {
  case 0:case 1:case 2:
    movetoorig(cc,x,y);
    rmoveto(cc,.03,h*1.25);
    break;
  case 3:case 4:
    vxcoords(&x0,&y0,x,y,3);
    rtop(&r0,&t0,x0,y0);
    r0-=.05;t0+=0.1/r0;
    if     (t0<=  PI/2) {r0-=w*sin(t0);ptor(&x0,&y0,r0,t0);           y0+=h*1.25;}
    else if(t0<=  PI)   {r0+=h*cos(t0);ptor(&x0,&y0,r0,t0);x0-=w+0.03;y0+=h*1.25;}
    else if(t0<=3*PI/2) {r0+=w*sin(t0);ptor(&x0,&y0,r0,t0);x0-=w+0.03;           }
    else                {r0-=h*cos(t0);ptor(&x0,&y0,r0,t0);                      }
    moveto(cc,x0,y0);
    break;
    }
  }





/*
Surfaces that make up main grid drawing area:

  sfb       : background
  sfc       : cursor
  sfs       : selected cells/lights
  sfh       : hint letters, hotspots
  sfn       : numbers
  sfq[x][y] : one for each square, containing entry letter, number, edges/bars, block, cutout
*/

cairo_surface_t*sfb=0,*sfc=0,*sfs=0,*sfh=0,*sfn=0,*sfq[MXSZ][MXSZ]={{0}};
int sfqx0[MXSZ][MXSZ],sfqy0[MXSZ][MXSZ],sfqx1[MXSZ][MXSZ],sfqy1[MXSZ][MXSZ]; // bounding boxes of squares

extern void draw_finit() {int x,y;
  if(sfb) cairo_surface_destroy(sfb);  sfb=0;
  if(sfc) cairo_surface_destroy(sfc);  sfc=0;
  if(sfs) cairo_surface_destroy(sfs);  sfs=0;
  if(sfn) cairo_surface_destroy(sfn);  sfn=0;
  if(sfh) cairo_surface_destroy(sfh);  sfh=0;
  for(x=0;x<MXSZ;x++) for(y=0;y<MXSZ;y++) {
    if(sfq[x][y]) cairo_surface_destroy(sfq[x][y]);  sfq[x][y]=0;
    }
  }

// call this at init and whenever
// - grid props change
// - zoom changes
extern void draw_init() {
  int x,y;
  float x0,y0,x1,y1;
  int sx0=0,sy0=0,sx1=0,sy1=0;
  int gx0=0,gy0=0,gx1=0,gy1=0;

  draw_finit(); // make sure everything is freed
  hbawdpx=((int)round(pxsq*BAWD/2.0));
  if(hbawdpx<2) hbawdpx=2;
  bawdpx=hbawdpx*2-1;
  DEB4 printf("bawdpx=%d hbawdpx=%d\n",bawdpx,hbawdpx);
  for(x=0;x<MXSZ;x++) for(y=0;y<MXSZ;y++) if(isingrid(x,y)) {
    getsqbbox(&x0,&y0,&x1,&y1,x,y);
    sx0=(int)floor(x0*pxsq-hbawdpx);
    sy0=(int)floor(y0*pxsq-hbawdpx);
    sx1=(int) ceil(x1*pxsq+hbawdpx);
    sy1=(int) ceil(y1*pxsq+hbawdpx);
    sfqx0[x][y]=sx0;
    sfqy0[x][y]=sy0;
    sfqx1[x][y]=sx1;
    sfqy1[x][y]=sy1;
    if(sx0<gx0) gx0=sx0;
    if(sy0<gy0) gy0=sy0;
    if(sx1>gx1) gx1=sx1;
    if(sy1>gy1) gy1=sy1;
    sfq[x][y]=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,sx1-sx0,sy1-sy0);
    }
  DEB4 printf("Overall bbox = (%d,%d)-(%d,%d)\n",gx0,gy0,gx1,gy1);
  sfb=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gx1,gy1); // take top left as (0,0)
  sfc=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,pxsq,pxsq);
  sfs=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gx1,gy1);
  sfh=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gx1,gy1);
  sfn=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gx1,gy1);
  }

// Refresh drawing area: called here with clip already set on cr, so we can
// just repaint everything.
void repaint(cairo_t*cc) {int i,j;
  assert(sfb);
  cairo_set_source_surface(cc,sfb,bawdpx,bawdpx); // paint background
  cairo_paint(cc);
  cairo_set_source_surface(cc,sfc,sfcxoff,sfcyoff); // paint cursor
  cairo_paint(cc);
  cairo_set_source_surface(cc,sfh,bawdpx,bawdpx); // paint hints
  cairo_paint(cc);
  for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) if(isingrid(i,j)) { // paint each square
    assert(sfq[i][j]);
    cairo_set_source_surface(cc,sfq[i][j],sfqx0[i][j]+bawdpx,sfqy0[i][j]+bawdpx);
    cairo_paint(cc);
    }
  if(shownums) cairo_set_source_surface(cc,sfn,bawdpx,bawdpx); // paint numbers
  cairo_paint(cc);
  cairo_set_source_surface(cc,sfs,bawdpx,bawdpx); // paint selection
  cairo_paint(cc);
  }


static unsigned int hpat[]={ // hatch pattern
  0x808080, 0xcccccc, 0xffffff, 0xffffff, 0xffffff, 0xcccccc, 0x808080, 0x808080,
  0xcccccc, 0xffffff, 0xffffff, 0xffffff, 0xcccccc, 0x808080, 0x808080, 0x808080, 
  0xffffff, 0xffffff, 0xffffff, 0xcccccc, 0x808080, 0x808080, 0x808080, 0xcccccc, 
  0xffffff, 0xffffff, 0xcccccc, 0x808080, 0x808080, 0x808080, 0xcccccc, 0xffffff, 
  0xffffff, 0xcccccc, 0x808080, 0x808080, 0x808080, 0xcccccc, 0xffffff, 0xffffff, 
  0xcccccc, 0x808080, 0x808080, 0x808080, 0xcccccc, 0xffffff, 0xffffff, 0xffffff, 
  0x808080, 0x808080, 0x808080, 0xcccccc, 0xffffff, 0xffffff, 0xffffff, 0xcccccc, 
  0x808080, 0x808080, 0xcccccc, 0xffffff, 0xffffff, 0xffffff, 0xcccccc, 0x808080}; 

// ex=1 (export mode): don't stroke around edge, don't shade cutouts
void drawsqbg(cairo_t*cc,int x,int y,int sq,int ex) {int f,bg;
  f=getflags(x,y);
  bg=getbgcol(x,y);
  if(f&8) { // hatching for cutout
    if(ex) return;
    cairo_surface_t*cs;
    cairo_pattern_t*cp;
    cairo_matrix_t cm;
    gsave(cc);
    cs=cairo_image_surface_create_for_data((unsigned char*)hpat,CAIRO_FORMAT_RGB24,8,8,32);
    cp=cairo_pattern_create_for_surface(cs);
    cairo_matrix_init_scale(&cm,sq,sq);
    cairo_pattern_set_matrix(cp,&cm);
    cairo_set_source(cc,cp);
    cairo_pattern_set_extend(cairo_get_source(cc),CAIRO_EXTEND_REPEAT);
    sqpath(cc,x,y);
    fill(cc);
    cairo_pattern_destroy(cp);
    cairo_surface_destroy(cs);
    grestore(cc);
  } else {
    sqpath(cc,x,y);
    if(f&1) setrgbcolor(cc,0,0,0); // solid block? then black
    else setrgbcolor24(cc,bg); // otherwise bg
    setlinewidth(cc,1.0/sq);
    if(!ex) strokepreserve(cc);
    fill(cc);
    }
  }

// draw bars and edges 
// lf=1 draw entered letter also
void drawsqfg(cairo_t*cc,int x,int y,int sq,int ba,int lf) {
  int b,c,d,f,l,m,md,xr,yr;
  char s[10];
  float sc;

  // edges and bars
  b=0; for(d=0;d<ndir[gtype]*2;d++) if(isbar  (x,y,d)) b|=1<<d;
  m=0; for(d=0;d<ndir[gtype]*2;d++) if(ismerge(x,y,d)) m|=1<<d;
  setrgbcolor(cc,0,0,0);
  if(sqexists(x,y)) {
    setlinewidth(cc,1.0/pxsq);
    edges(cc,x,y,~m);
    stroke(cc);
    }
  setlinewidth(cc,ba/(double)sq);
  edges(cc,x,y,b);
  stroke(cc);
  // entered letter
  getmergerep(&xr,&yr,x,y);
  l=getmergegroup(0,0,xr,yr);
  md=getmergedir(xr,yr);
  if(md<0) md=0;
  f=getflags(xr,yr);
  sc=getscale(xr,yr,0,1);
  c=getechar(xr,yr);
  if((f&9)==0&&lf) {
    s[0]=c;s[1]=0;
    setrgbcolor24(cc,getfgcol(xr,yr));
    movetomgcentre(cc,xr,yr,md,l);
    rmoveto(cc,0,0.3*sc);
    ctext(cc,s,.7*sc);
    }
  }

// Draw one square into sfq[x][y].
void refreshsq(int x,int y) {cairo_t*cc;
  DEB4 printf("refreshsq(%d,%d)\n",x,y);
  assert(sfq[x][y]);
  // background
  cc=cairo_create(sfb);
  cairo_translate(cc,0.5,0.5);
  cairo_scale(cc,pxsq,pxsq);
  drawsqbg(cc,x,y,pxsq,0);
  cairo_destroy(cc);
  // foreground
  cc=cairo_create(sfq[x][y]);
  cairo_set_source_rgba(cc,0.0,0.0,0.0,0.0);
//DEB4 cairo_set_source_rgba(cc,drand48(),drand48(),drand48(),0.5);
  cairo_set_operator(cc,CAIRO_OPERATOR_SOURCE);
  cairo_paint(cc);
  cairo_set_operator(cc,CAIRO_OPERATOR_OVER);
  cairo_translate(cc,-sfqx0[x][y]+0.5,-sfqy0[x][y]+0.5);
  cairo_scale(cc,pxsq,pxsq);
  drawsqfg(cc,x,y,pxsq,bawdpx,1);
  cairo_destroy(cc);
  invaldarect(sfqx0[x][y]+bawdpx,sfqy0[x][y]+bawdpx,sfqx1[x][y]+bawdpx,sfqy1[x][y]+bawdpx);
  }

void refreshsqlist(int l,int*gx,int*gy) {int i;
  DEB4 printf("refreshsqlist([%d])\n",l);
  for(i=0;i<l;i++) refreshsq(gx[i],gy[i]);
  }

void refreshsqmg(int x,int y) {int l,gx[MXSZ],gy[MXSZ];
  DEB4 printf("refreshsqmg(%d,%d)\n",x,y);
  l=getmergegroup(gx,gy,x,y);
  refreshsqlist(l,gx,gy);
  }

void refreshsel() {int d,i,j,k,l,lx[MXSZ],ly[MXSZ],tl; float u,v,u0,v0; cairo_t*cc;
  cc=cairo_create(sfs);
  cairo_set_source_rgba(cc,0.0,0.0,0.0,0.0);
  cairo_set_operator(cc,CAIRO_OPERATOR_SOURCE);
  cairo_paint(cc);
  cairo_set_operator(cc,CAIRO_OPERATOR_OVER);
  cairo_translate(cc,0.5,0.5);
  cairo_scale(cc,pxsq,pxsq);
  cairo_set_source_rgba(cc,1.0,0.8,0.0,0.5);
  if(selmode==0) { // fill selected cells
    for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) if(isingrid(i,j)&&(getflags(i,j)&0x10)) {
      DEB4 printf("sel fill %d,%d\n",i,j);
      sqpath(cc,i,j);
      fill(cc);
      }
  } else { // refresh selected lights
    setlinecap(cc,1);
    setlinewidth(cc,.5);
    for(j=0;j<height;j++) for(i=0;i<width;i++) for(d=0;d<ndir[gtype];d++)
      if(isstartoflight(i,j,d)&&issellight(i,j,d)) {
        l=getlightd(lx,ly,i,j,d);
        assert(l>=2);
        tl=getmergegroupd(0,0,lx[0],ly[0],d);
        movetomgcentre(cc,lx[0],ly[0],d,tl);
        mgcentre(&u,&v,lx[0],ly[0],d,tl);
        for(k=1;k<l;k++) {
          u0=u; v0=v;
          tl=getmergegroupd(0,0,lx[k],ly[k],d);
          mgcentre(&u,&v,lx[k],ly[k],d,tl);
          DEB4 printf("%f,%f (%f) - %f,%f (%f)\n",u0,v0,atan2(v0,u0),u,v,atan2(v,u));
          if((gtype==3||gtype==4)&&d==0) {
            arc(cc,height,height,height-ly[k]-0.5,atan2(v0-height,u0-height),atan2(v-height,u-height));
            } else lineto(cc,u,v);
          }
        stroke(cc);
        }
    setlinecap(cc,0);
    }
  cairo_destroy(cc);
  invaldaall();
  }

void refreshcur() {
  float sc,c=0,s=0,u,v;
  int x,y;
  static int ocx=0,ocy=0;
  cairo_t*cc;

  cc=cairo_create(sfc);
  cairo_set_source_rgba(cc,0.0,0.0,0.0,0.0);
  cairo_set_operator(cc,CAIRO_OPERATOR_SOURCE);
  cairo_paint(cc);
  cairo_set_operator(cc,CAIRO_OPERATOR_OVER);
  sc=getscale(curx,cury,0,1)*pxsq;
  getdxdy(&c,&s,curx,cury,dir);c*=sc;s*=sc;
  setrgbcolor(cc,0.6,0.6,0.6);
  moveto(cc,pxsq/2,pxsq/2);
  rmoveto(cc,c* .4      ,      s* .4);
  rlineto(cc,c*-.8+s*-.3,c* .3+s*-.8);
  rlineto(cc,      s* .6,c*-.6      );
  closepath(cc);
  fill(cc);
  mgcentre(&u,&v,curx,cury,0,1);
  x=(int)round(u*pxsq); y=(int)round(v*pxsq);
  sfcxoff=x-pxsq/2+bawdpx;
  sfcyoff=y-pxsq/2+bawdpx;
  invaldarect(sfcxoff,sfcyoff,sfcxoff+pxsq,sfcyoff+pxsq);
  mgcentre(&u,&v,ocx,ocy,0,1);
  x=(int)round(u*pxsq); y=(int)round(v*pxsq);
  invaldarect(x-pxsq/2+bawdpx,y-pxsq/2+bawdpx,x+pxsq/2+bawdpx,y+pxsq/2+bawdpx);
  ocx=curx;ocy=cury;
  cairo_destroy(cc);
  }

void drawnums(cairo_t*cc) {int x,y,n; float sc; char s[10];
  for(y=0;y<MXSZ;y++) for(x=0;x<MXSZ;x++) if(isingrid(x,y)) {
    n=getnumber(x,y);
    sc=getscale(x,y,0,1);
    if(n>=0) { // draw a number?
      sprintf(s,"%d",n);
      movetonum(cc,x,y,textwidth(cc,s,.25*sc)+.1*sc,.27*sc);
      rmoveto(cc,.05*sc,-.05*sc);
      setrgbcolor(cc,0,0,0);
      ltext(cc,s,.25*sc);
      }
    }
  }

void refreshnum() {cairo_t*cc;
  cc=cairo_create(sfn);
  cairo_set_source_rgba(cc,0.0,0.0,0.0,0.0);
  cairo_set_operator(cc,CAIRO_OPERATOR_SOURCE);
  cairo_paint(cc);
  cairo_set_operator(cc,CAIRO_OPERATOR_OVER);
  cairo_translate(cc,0.5,0.5);
  cairo_scale(cc,pxsq,pxsq);
  setrgbcolor(cc,0,0,0);
  drawnums(cc);
  cairo_destroy(cc);
  invaldaall();
  }

void refreshhin() {
  int f,k,l,md,x,y;
  ABM m;
  char c;
  char s[SLEN];
  float sc,u;
  cairo_t*cc;

  cc=cairo_create(sfh);
  cairo_set_source_rgba(cc,0.0,0.0,0.0,0.0);
  cairo_set_operator(cc,CAIRO_OPERATOR_SOURCE);
  cairo_paint(cc);
  cairo_set_operator(cc,CAIRO_OPERATOR_OVER);
  cairo_translate(cc,0.5,0.5);
  cairo_scale(cc,pxsq,pxsq);
  setrgbcolor(cc,0,0,0);
  for(y=0;y<MXSZ;y++) for(x=0;x<MXSZ;x++) if(isingrid(x,y)&&isownmergerep(x,y)) {
    l=getmergegroup(0,0,x,y);
    md=getmergedir(x,y);
    if(md<0) md=0;
    f=getflags(x,y);
    sc=getscale(x,y,0,1);
    if((f&9)==0&&getechar(x,y)==' ') { // empty square
      m=getflbm(x,y);k=cbits(m); // any info from filler?
      DEB4 printf("%16llx ",m);
      if(k==0) c='?'; // no feasible letters
      else if(k==1) c=ltochar[logbase2(m)]; // unique feasible letter
      else c=' ';
      s[0]=c;s[1]=0;
      setrgbcolor(cc,0.75,0.75,0.75);
      movetomgcentre(cc,x,y,md,l);
      rmoveto(cc,0,0.3*sc);
      ctext(cc,s,.7*sc);
      if(k>1&&k<pxsq/2) { // more than one feasible letter
        u=1.0/k*sc; // size of red square
        setrgbcolor(cc,1,0,0);
        movetomgcentre(cc,x,y,md,l);
        rmoveto(cc,u/2.0,u/2.0);
        rlineto(cc,0,-u);
        rlineto(cc,-u,0);
        rlineto(cc,0,u);
        closepath(cc);
        fill(cc);
        }
      }
    }
  cairo_destroy(cc);
  invaldaall();
  }

void refreshall() {int i,j;
  for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) if(isingrid(i,j)) {refreshsq(i,j);}
  refreshcur();
  if(shownums) refreshnum();
  refreshhin();
  refreshsel();
  }


int gwidth(int sq,int ba) {
  switch(gtype) {
  case 0:return  width         *sq+ba*2-1;
  case 1:return (width*1.2+0.4)*sq+ba*2-1;
  case 2:if(EVEN(height)) return (width*1.4+0.7)*sq+ba*2-1;
         else             return (width*1.4    )*sq+ba*2-1;
  case 3:case 4:return height*sq*2+ba*2-1;
    }
  assert(0);
  }

int gheight(int sq,int ba) {
  switch(gtype) {
  case 0:return height*sq+ba*2-1;
  case 1:if(EVEN(width)) return (height*1.4+0.7)*sq+ba*2-1;
         else            return (height*1.4    )*sq+ba*2-1;
  case 2:return (height*1.2+0.4)*sq+ba*2-1;
  case 3:case 4:return height*sq*2+ba*2-1;
    }
  assert(0);
  }

int dawidth(void)  {return gwidth (pxsq,bawdpx);}
int daheight(void) {return gheight(pxsq,bawdpx);}


// EXPORT FUNCTIONS

// output string escaped so that it will reproduce correctly in HTML
static void escstr(FILE*fp,char*p) {int i;
  for(i=0;p[i];p++) {
    if(!isprint(p[i])) continue; // printable?
    switch(p[i]) {
      case'&': fprintf(fp,"&amp;");break;
      case'\"':fprintf(fp,"&quot;");break;
      case'<': fprintf(fp,"&lt;");break;
      case'>': fprintf(fp,"&gt;");break;
      case'|': fprintf(fp,"&#124;");break;
      default: fputc(p[i],fp);break;
      }
    }
  }

// HTML export
//  f b0: include grid
//  f b1: include answers in grid
//  f b2: include numbers in grid
//  f b3: include answers in table (to be edited into clues)
//  f b4: include answers in compact format
//  f b5: include link to grid image (given URL)
// Thanks to Paul Boorer for CSS suggestions
void a_exportgh(int f,char*url) {
  int d,fl,i,j,k,l,md,u,v,x,y,bg,fg;char t[MXSZ+1];
  char ch;
  int hbawd;
  FILE*fp;

  hbawd=hpxsq/6; // a suitable bar width
  if(hbawd<2) hbawd=2;
  DEB1 printf("export HTML %s\n",filename);
  fp=fopen(filename,"w");
  if(!fp) {fserror();return;}
  // emit header and CSS
  fprintf(fp,"\
  <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n\
  \"http://www.w3.org/TR/REC-html40/loose.dtd\">\n\
  <html>\n\
  <!-- Created by Qxw %s http://www.quinapalus.com -->\n\
  <head>\n\
  <title>",RELEASE);
  escstr(fp,titlebyauthor());
  fprintf(fp,"</title>\n");
  if(f&1) { // need the styles for the grid
    fprintf(fp,"<style type=\"text/css\">\n");
    fprintf(fp,"div.bk {position:absolute;font-size:0px;border-left:black 0px solid;border-right:black 0px solid;border-top:black %dpx solid;border-bottom:black 0px solid;width:%dpx;height:%dpx;}\n",hpxsq,hpxsq,hpxsq);
    fprintf(fp,"div.hb {position:absolute;font-size:0px;border-left:black 0px solid;border-right:black 0px solid;border-top:black %dpx solid;border-bottom:black 0px solid;width:%dpx;}\n",hbawd,hpxsq);
    fprintf(fp,"div.vb {position:absolute;font-size:0px;border-left:black %dpx solid;border-right:black 0px solid;border-top:black 0px solid;border-bottom:black 0px solid;height:%dpx;}\n",hbawd,hpxsq);
    fprintf(fp,"div.hr {position:absolute;font-size:0px;border-left:black 0px solid;border-right:black 0px solid;border-top:black 1px solid;border-bottom:black 0px solid;}\n");
    fprintf(fp,"div.hr {position:absolute;font-size:0px;border-left:black 0px solid;border-right:black 0px solid;border-top:black 1px solid;border-bottom:black 0px solid;}\n");
    fprintf(fp,"div.vr {position:absolute;font-size:0px;border-left:black 1px solid;border-right:black 0px solid;border-top:black 0px solid;border-bottom:black 0px solid;}\n");
    fprintf(fp,"div.nu {position:absolute;font-size:%dpx;font-family:sans-serif}\n",hpxsq/3);
    fprintf(fp,"div.lt {position:absolute;text-align:center;font-size:%dpx;width:%dpx;font-family:sans-serif}\n",hpxsq*3/4,hpxsq);
    fprintf(fp,"</style>\n");
    }
  fprintf(fp,"</head>\n\n<body>\n");
  if(f&1) {
    fprintf(fp,"<center>\n");
    fprintf(fp,"<div style=\"border-width:0px;width:%dpx;height:%dpx;position:relative;\">\n",width*hpxsq+1,height*hpxsq+1);
    fprintf(fp,"<div style=\"border-width:0px;top:0px;left:0px;width:%dpx;height:%dpx;position:absolute;\">\n",width*hpxsq+1,height*hpxsq+1);
    for(j=0;j<height;j++) for(i=0;i<width;i++) {
      bg=getbgcol(i,j);
      fl=getflags(i,j);
      if(fl&1)                      {fprintf(fp,"<div class=\"bk\" style=\"left:%dpx;top:%dpx;\"></div>\n",i*hpxsq+1,j*hpxsq+1);continue;}
      fprintf(fp,"<div class=\"bk\" style=\"left:%dpx;top:%dpx;border-top:#%06X %dpx solid;\"></div>\n",i*hpxsq+1,j*hpxsq+1,bg,hpxsq);
      if(i<width -1&&isbar(i,j,0))   fprintf(fp,"<div class=\"vb\" style=\"left:%dpx;top:%dpx;\"></div>\n",(i+1)*hpxsq-hbawd+1,j*hpxsq+1);
      if(j<height-1&&isbar(i,j,1))   fprintf(fp,"<div class=\"hb\" style=\"left:%dpx;top:%dpx;\"></div>\n",i*hpxsq+1,(j+1)*hpxsq-hbawd+1);
      if((f&4)&&gsq[i][j].number>-1) fprintf(fp,"<div class=\"nu\" style=\"left:%dpx;top:%dpx;\">%d</div>\n",i*hpxsq+2,j*hpxsq+1,gsq[i][j].number);
      }
    for(j=0;j<height;j++) for(i=0;i<width;i++) {
      if((f&2)&&isingrid(i,j)&&isownmergerep(i,j)) {
        fg=getfgcol(i,j);
        l=getmergegroup(0,0,i,j);
        md=getmergedir(i,j);
        if(md<0) md=0;
        ch=getechar(i,j);
        x=((i*2+(md==0)*(l-1))*hpxsq)/2+1;
        y=((j*2+2+(md==1)*(l-1))*hpxsq)/2-hpxsq*3/4-hbawd;
        if(ch!=' ') fprintf(fp,"<div class=\"lt\" style=\"left:%dpx;top:%dpx;color:%06X;\">%c</div>\n",x,y,fg,ch);
        }
      }
    for(j=0;j<=height;j++) for(i=0,v=0;i<=width;i++) {
      u=(sqexists(i,j)|sqexists(i,j-1))&!ismerge(i,j-1,1);
      if(u==0&&v>0) fprintf(fp,"<div class=\"hr\" style=\"left:%dpx;top:%dpx;width:%dpx;\"></div>\n",(i-v)*hpxsq,j*hpxsq,v*hpxsq+1);
      if(u==0) v=0; else v++;
      }
    for(i=0;i<=width;i++) for(j=0,v=0;j<=height;j++) {
      u=(sqexists(i,j)|sqexists(i-1,j))&!ismerge(i-1,j,0);
      if(u==0&&v>0) fprintf(fp,"<div class=\"vr\" style=\"left:%dpx;top:%dpx;height:%dpx;\"></div>\n",i*hpxsq,(j-v)*hpxsq,v*hpxsq+1);
      if(u==0) v=0; else v++;
      }
    fprintf(fp,"</div></div></center>\n");
    }
  if(f&0x20) {
    fprintf(fp,"<center>\n");
    fprintf(fp,"<img src=\"%s\" alt=\"grid image\">\n",url);
    fprintf(fp,"</center>\n");
    }

  if(f&8) { // print table of `clues'
    fprintf(fp,"<center><table><tr>\n");
    for(d=0;d<ndir[gtype];d++) {
      fprintf(fp,"<td width=\"%d%%\"><b>%s</b></td>\n",96/ndir[gtype],dname[gtype][d]);
      if(d<ndir[gtype]-1) fprintf(fp,"<td>&nbsp;</td>\n");
      }
    fprintf(fp,"</tr><tr>\n");
    for(d=0;d<ndir[gtype];d++) {
      fprintf(fp,"<td valign=top><table>\n");
      for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) {
        getword(i,j,d,t);
        for(k=0;t[k];k++) if(t[k]==' ') t[k]='.';
        fprintf(fp,"<tr><td width=0 align=right valign=top><b>%d</b></td>\n",gsq[i][j].number); // clue number
        fprintf(fp,"<td valign=top>%s (%d)</td></tr>\n",t,(int)strlen(t)); // answer and length
        }
      fprintf(fp,"</table></td>\n");
      if(d<ndir[gtype]-1) fprintf(fp,"<td>&nbsp;</td>\n");
      }
    fprintf(fp,"</tr></table></center>\n");
    }

  if(f&16) { // print answers in condensed form
    for(d=0;d<ndir[gtype];d++) {
      fprintf(fp,"<b>%s:</b>\n",dname[gtype][d]);
      for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) {
        getword(i,j,d,t);
        for(k=0;t[k];k++) if(t[k]==' ') t[k]='.';
        fprintf(fp,"<b>%d</b> %s\n",gsq[i][j].number,t);
        }
      fprintf(fp,"<br>\n");
      }
    }

  fprintf(fp,"</body></html>\n");
  if(ferror(fp)) fserror();
  if(fclose(fp)) fserror();
  }


// GRID AS EPS OR PNG

// f0 b1: include answers in grid
// f0 b2: include numbers in grid
// f1: 0=EPS 1=PNG 
void a_exportg(char*fn,int f0,int f1) {
  cairo_surface_t*sf=0;
  cairo_t*cc;
  int i,j,sq=0,ba,px,py;
  char s[1000];
  FILE*fp=0;

  cairo_status_t fwr(FILE*fp,unsigned char*p,unsigned int n) {
    return (fwrite(p,1,n,fp)==n)?CAIRO_STATUS_SUCCESS:CAIRO_STATUS_WRITE_ERROR;
    }

  switch(f1) {
  case 0: sq=eptsq; break;
  case 1: sq=hpxsq; break;
    }
  ba=sq/8; // a suitable bar width
  if(ba<2) ba=2;
  px=gwidth(sq,ba);
  py=gheight(sq,ba);
  switch(f1) {
  case 0:
    fp=fopen(fn,"w");
    if(!fp) {fserror();return;}
    sf=cairo_ps_surface_create_for_stream((cairo_write_func_t)fwr,fp,px,py);
    cairo_ps_surface_restrict_to_level(sf,CAIRO_PS_LEVEL_2);
    cairo_ps_surface_set_eps(sf,1);
    sprintf(s,"%%%%Title: %s generated by Qxw",fn);
    cairo_ps_surface_dsc_comment(sf,s);
    break;
  case 1:
    sf=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,px,py);
    break;
    }
  cc=cairo_create(sf);
  if(f1==1) {
    cairo_set_source_rgb(cc,1,1,1); // flood PNG to white to avoid nasty transparency effect in merged cells
    cairo_paint(cc);
    }
  cairo_translate(cc,ba-.5,ba-.5);
  cairo_scale(cc,sq,sq);
  for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) if(isingrid(i,j)) {
    drawsqbg(cc,i,j,sq,1);
    }
  for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) if(isingrid(i,j)) {
    gsave(cc);
    drawsqfg(cc,i,j,sq,ba,!!(f0&2));
    grestore(cc);
    }
  setrgbcolor(cc,0,0,0);
  if(f0&4) drawnums(cc);
  switch(f1) {
  case 0: 
    cairo_show_page(cc);
    break;
  case 1: 
    if(cairo_surface_write_to_png(sf,fn)!=CAIRO_STATUS_SUCCESS) fsgerr();
    break;
    }
  cairo_destroy(cc);
  cairo_surface_destroy(sf);
  switch(f1) {
  case 0: 
    if(ferror(fp)) {fserror();fclose(fp);}
    else if(fclose(fp)) fserror();
    break;
  case 1: 
    break;
    }
  }


// ANSWERS

// f: 0=text, 1=HTML
void a_exporta(int f) {
  int d,i,j,k;char t[MXSZ+1];
  FILE*fp;

  fp=fopen(filename,"w");
  if(!fp) {fserror();return;}
  fprintf(fp,f?"<html><body><!-- ":"# ");
  fprintf(fp,"file %s generated by Qxw %s: http://www.quinapalus.com",filename,RELEASE);
  fprintf(fp,f?" -->\n":"\n");
  for(d=0;d<ndir[gtype];d++) {
    if(f) fprintf(fp,"\n<b>%s</b><br>\n\n",dname[gtype][d]);
    else  fprintf(fp,"\n%s\n\n",dname[gtype][d]);
    for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) {
      getword(i,j,d,t);
      for(k=0;t[k];k++) if(t[k]==' ') t[k]='.';
      if(f) fprintf(fp,"<b>%d</b> %s (%d)<br>\n",gsq[i][j].number,t,(int)strlen(t));
      else  fprintf(fp,"%d %s (%d)\n",gsq[i][j].number,t,(int)strlen(t));
      }
    }
  if(f) fprintf(fp,"</body></html>\n");
  if(ferror(fp)) fserror();
  if(fclose(fp)) fserror();
  }

// COMBINED HTML AND PNG

// f: 0=puzzle, 1=solution
void a_exporthp(int f) {char url[SLEN+200];
  strcpy(url,filename);
  strcat(url,"_files");
  mkdir(url,0777);
  strcat(url,f?"/solution.png":"/grid.png");
  a_exportgh(f?0x30:0x28,url);
  a_exportg(url,f?2:4,1);
  }
