# #####################################################################
# FSE - tester Makefile
# Copyright (C) Yann Collet 2011-2013
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - Public forum froup : https://groups.google.com/forum/#!forum/lz4c
# #####################################################################
# fse : Command Line Utility, featuring benchmark and validation tools
# #####################################################################

RELEASE=v0.1
DESTDIR=
CC=gcc
CFLAGS=-I.. -std=c99 -Wall -W -Wundef -O3
LDFLAGS=-lm
CF32=-m32 -march=pentiumpro

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif


default: fse fse_custom

all: default fse32 fuzzer probagen

fse: bench.o commandline.o fileio.o lz4hce.o xxhash.o fseDist.o fse2t.o zlibh.o ../fse.o
	$(CC) $(LDFLAGS) $^ -o $@$(EXT)

fse_custom.o: CFLAGS+=-DSPREADFUNC=custom_spread
 
fse.o fse_custom.o: ../fse.c ../fse.h
	$(CC) $(CFLAGS) -c $< -o $@

fse_custom: bench.o commandline.o fileio.o lz4hce.o xxhash.o fseDist.o fse2t.o zlibh.o custom_spread.o fse_custom.o
	$(CC) $(LDFLAGS) $^ -o $@$(EXT)

fse32: bench.c commandline.c fileio.c lz4hce.c xxhash.c fseDist.c fse2t.c zlibh.c ../fse.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@$(EXT) $(CF32)

fuzzer: fuzzer.o xxhash.o ../fse.o
	$(CC) $(LDFLAGS) $^ -o $@$(EXT)

probagen: probaGenerator.o
	$(CC) $(LDFLAGS) $^ -o $@$(EXT)

clean:
	@rm -f core *.o fse$(EXT) fse32$(EXT) fuzzer$(EXT) probagen$(EXT) fse_custom$(EXT)
	@echo Cleaning completed

