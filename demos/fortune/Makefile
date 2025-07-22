# *************************** FILE HEADER ***********************************
#
#  \file       Makefile
#
#  \brief      <b>Makefile for core object.</b>\n
#
#              This module is called from either a launch point in an
#              html page or from the core_authoring program.\n
#
#  \note       Some special notes
#
#  \author     Andrew Sporner\n
#
#  \version    1   2010-11-05 Andrew Sporner    created
#
# *************************** FILE HEADER **********************************/

#
#  CFLAGS
#
CFLAGS+=					-Wall -g


#
#  OBJECT LIST
#
SERVICE_OBJS=				app_main.o										\
							../../src/service.o

#
#  TARGETS
#

all: $(SERVICE_OBJS) 
	$(CC) -o service $(SERVICE_OBJS) $(LDFLAGS)

clean:
		rm -f *.o service

