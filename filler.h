// $Id: filler.h -1   $

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

#ifndef __FILLER_H__
#define __FILLER_H__

// control functions for filler called from main code
extern int filler_start(void);
extern int filler_step(void);
extern void filler_stop(void);
extern void getposs(struct entry*e,char*s,int r);

#endif
