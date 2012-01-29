/* $Id: qxw.h -1   $ */

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

extern int dir,curx,cury;
extern int unsaved;
extern char bname[SLEN];
extern char filename[SLEN+50];
extern int symmr,symmd,symmm;
extern int gtype,ndir[NGTYPE];
extern int width,height;
extern char gtitle[SLEN];
extern char gauthor[SLEN];

extern int uhead,utail,uhwm;

extern int st_lc[MXSZ+1];
extern int st_lucc[MXSZ+1];
extern int st_locc[MXSZ+1];
extern int st_lsc[MXSZ+1];
extern int st_lmnc[MXSZ+1];
extern int st_lmxc[MXSZ+1];
extern int st_sc;
extern int st_ce;
extern int st_2u,st_3u;

extern struct sprop dsp;
extern struct lprop dlp;

// functions called by grid filler
extern void updatefeas(void);
extern void updategrid(void);
extern void mkfeas(void);

// interface functions to gsq[][]
extern int getflags(int x,int y);
extern int getbgcol(int x,int y);
extern int getfgcol(int x,int y);
extern int getnumber(int x,int y);
extern int getechar(int x,int y);
extern ABM getflbm(int x,int y);
extern void a_load(void);
extern void a_save(void);
extern void a_filenew(void);
extern void saveprefs(void);
extern void a_editblock (int k,int x,int y,int d);
extern void a_editcutout(int k,int x,int y,int d);
extern void a_editspace (int k,int x,int y,int d);
extern void a_editmerge (int k,int x,int y,int d);
extern void a_editbar   (int k,int x,int y,int d);
extern int symmrmask(void);
extern int symmmmask(void);
extern int symmdmask(void);
extern int cbits(ABM x);
extern int logbase2(ABM x);
extern void undo_push(void);
extern void undo_pop(void);
extern int getlightd(int*lx,int*ly,int x,int y,int d);
extern int getstartoflight(int*lx,int*ly,int x,int y,int d);
extern int getlight(int*lx,int*ly,int x,int y,int d);
extern int isstartoflight(int x,int y,int d);
extern int issellight(int x,int y,int d);
extern void sellight(int x,int y,int d,int k);
extern int isclear(int x,int y);
extern int isbar(int x,int y,int d);
extern int ismerge(int x,int y,int d);
extern int isingrid(int x,int y);
extern int sqexists(int i,int j);
extern int clearbefore(int x,int y,int d);
extern int clearafter(int x,int y,int d);
extern int getword(int x,int y,int d,char*s);
extern int getmergegroupd(int*gx,int*gy,int x,int y,int d);
extern int getmergegroup(int*gx,int*gy,int x,int y);
extern int getmergedir(int x,int y);
extern void getmergerep(int*mx,int*my,int x,int y);
extern int isownmergerep(int x,int y);
extern void getmergerepd(int*mx,int*my,int x,int y,int d);
extern void compute(void);
extern void symmdo(void f(int,int,int,int),int k,int x,int y,int d);
extern void setch(int x,int y,char c);
extern void stepback(int*x,int*y,int d);
extern void donumbers(void);
extern int stepbackifingrid (int*x,int*y,int d);
//extern int stepforwifingrid (int*x,int*y,int d);
extern int stepforwmifingrid(int*x,int*y,int d);
extern char*titlebyauthor(void);
