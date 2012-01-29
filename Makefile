## REL+

# Copyright 2011 Mark Owen
# http://www.quinapalus.com
# E-mail: qxw@quinapalus.com
#
# This file is part of Qxw.
#
# Qxw is free software: you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License
# as published by the Free Software Foundation.
#
# Qxw is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Qxw.  If not, see <http://www.gnu.org/licenses/> or
# write to the Free Software Foundation, Inc., 51 Franklin Street,
# Fifth Floor, Boston, MA  02110-1301, USA.


all: qxw

COPTS := -O9 -Wall
LFLAGS := -lgtk-x11-2.0 -lgdk-x11-2.0 -lm -lcairo -lgobject-2.0

qxw: qxw.o filler.o dicts.o gui.o draw.o
	gcc -rdynamic -Wall -ldl qxw.o filler.o dicts.o gui.o draw.o $(LFLAGS) -o qxw
#	gcc -rdynamic -Wall -ldl qxw.o filler.o dicts.o gui.o draw.o `pkg-config --libs gtk+-2.0` -o qxw

qxw.o: qxw.c qxw.h filler.h dicts.h draw.h gui.h common.h
	gcc $(COPTS) -c qxw.c `pkg-config --cflags gtk+-2.0` -o qxw.o

gui.o: gui.c qxw.h filler.h dicts.h draw.h gui.h common.h
	gcc $(COPTS) -c gui.c `pkg-config --cflags gtk+-2.0` -o gui.o

filler.o: filler.c qxw.h filler.h dicts.h common.h
	gcc $(COPTS) -c filler.c -o filler.o

dicts.o: dicts.c dicts.h gui.h common.h
	gcc $(COPTS) -fno-strict-aliasing -c dicts.c -o dicts.o

draw.o: draw.c qxw.h draw.h gui.h common.h
	gcc $(COPTS) -c draw.c `pkg-config --cflags gtk+-2.0` -o draw.o

.PHONY: clean
clean:
	rm -f dicts.o draw.o filler.o gui.o qxw.o qxw

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/games
	cp -a qxw $(DESTDIR)/usr/games/qxw
	mkdir -p $(DESTDIR)/usr/include/qxw
	cp -a qxwplugin.h $(DESTDIR)/usr/include/qxw/qxwplugin.h
	mkdir -p $(DESTDIR)/usr/share/applications
	cp -a qxw.desktop $(DESTDIR)/usr/share/applications/qxw.desktop

## REL-
