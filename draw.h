// $Id: draw.h -1   $

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





#ifndef __DRAW_H__
#define __DRAW_H__

extern int bawdpx;
extern int hbawdpx;

extern void draw_init();
extern void draw_finit();
extern void repaint(cairo_t*cr);

extern void refreshsq(int x,int y);
extern void refreshsqlist(int l,int*gx,int*gy);
extern void refreshsqmg(int x,int y);
extern void refreshsel();
extern void refreshcur();
extern void refreshnum();
extern void refreshhin();
extern void refreshall();

extern int dawidth(void);
extern int daheight(void);

extern void a_exportg(char*fn,int f0,int f1);
extern void a_exportgh(int f,char*url);
extern void a_exporta(int f);
extern void a_exporthp(int f);

extern void edgecoords(float*x0,float*y0,float*x1,float*y1,int x,int y,int d);
extern void addsqbbox(float*x0,float*y0,float*x1,float*y1,int x,int y);
extern void getsqbbox(float*x0,float*y0,float*x1,float*y1,int x,int y);
extern void getmgbbox(float*x0,float*y0,float*x1,float*y1,int x,int y);
extern void mgcentre(float*u,float*v,int x,int y,int d,int l);

#endif
