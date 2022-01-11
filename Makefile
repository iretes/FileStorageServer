CC = gcc
CFLAGS += -std=c99 -Wall -Werror -g -D_POSIX_C_SOURCE=2001012L
SHELL = /bin/bash
LIBSYS = -L. -lpthread

OBJDIR = ./obj
INCDIR = ./include
BINDIR = ./bin
SRCDIR = ./src

INCLUDES = -I $(INCDIR)

TARGETS	= $(BINDIR)/client $(BINDIR)/server

.PHONY: all clean cleanall 

.SUFFIXES: .c .h

all: $(TARGETS)

CLIENTOBJS = $(OBJDIR)/client.o 
SERVEROBJS = $(OBJDIR)/server.o $(OBJDIR)/hasht.o $(OBJDIR)/conc_hasht.o $(OBJDIR)/list.o $(OBJDIR)/int_list.o \
$(OBJDIR)/eviction_policy.o $(OBJDIR)/config_parser.o $(OBJDIR)/util.o $(OBJDIR)/threadpool.o $(OBJDIR)/logger.o

$(BINDIR)/client: $(CLIENTOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBSYS)

$(BINDIR)/server: $(SERVEROBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBSYS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/server.o: $(SRCDIR)/server.c $(INCDIR)/hasht.h $(INCDIR)/conc_hasht.h $(INCDIR)/list.h $(INCDIR)/int_list.h \
$(INCDIR)/eviction_policy.h $(INCDIR)/config_parser.h $(INCDIR)/util.h $(INCDIR)/protocol.h $(INCDIR)/threadpool.h \
$(INCDIR)/logger.h $(INCDIR)/log_format.h

$(OBJDIR)/client.o: $(SRCDIR)/client.c

$(OBJDIR)/hasht.o: $(SRCDIR)/hasht.c $(INCDIR)/hasht.h

$(OBJDIR)/conc_hasht.o: $(SRCDIR)/conc_hasht.c $(INCDIR)/hasht.h $(INCDIR)/conc_hasht.h

$(OBJDIR)/list.o: $(SRCDIR)/list.c $(INCDIR)/list.h

$(OBJDIR)/int_list.o: $(SRCDIR)/int_list.c $(INCDIR)/list.h $(INCDIR)/int_list.h

$(OBJDIR)/util.o: $(SRCDIR)/util.c $(INCDIR)/util.h

$(OBJDIR)/eviction_policy.o: $(SRCDIR)/eviction_policy.c $(INCDIR)/eviction_policy.h

$(OBJDIR)/config_parser.o: $(SRCDIR)/config_parser.c $(INCDIR)/config_parser.h $(INCDIR)/protocol.h \
$(INCDIR)/eviction_policy.h $(INCDIR)/util.h

$(OBJDIR)/threadpool.o: $(SRCDIR)/threadpool.c $(INCDIR)/threadpool.h $(INCDIR)/util.h

$(OBJDIR)/logger.o: $(SRCDIR)/logger.c $(INCDIR)/util.h $(INCDIR)/logger.h

clean: 
	@rm -f $(TARGETS)

cleanall: clean
	@rm -f $(OBJDIR)/*.o $(BINDIR)/*