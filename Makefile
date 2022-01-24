CC = gcc
CFLAGS += -std=c99 -Wall -Werror -g -D_POSIX_C_SOURCE=2001012L
SHELL = /bin/bash

SRCDIR = ./src
INCDIR = ./include
OBJDIR = ./obj
LIBDIR = ./lib
BINDIR = ./bin

INCLUDES = -I $(INCDIR)
TARGETS = $(BINDIR)/server $(BINDIR)/client

LIBSERVER = -llist -lhasht -lpool -llogger -lprotocol -lpthread
LIBCLIENT = -llist -lclientapi -lprotocol

SERVEROBJS = $(OBJDIR)/server.o \
    $(OBJDIR)/storage_server.o \
    $(OBJDIR)/eviction_policy.o \
    $(OBJDIR)/config_parser.o \
    $(OBJDIR)/util.o

CLIENTOBJS = $(OBJDIR)/client.o \
    $(OBJDIR)/cmdline_operation.o \
    $(OBJDIR)/cmdline_parser.o

.PHONY: all clean cleanall

all: $(TARGETS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC -o $@ $<

# FILE ESEGUIBILI

$(BINDIR)/server: $(SERVEROBJS) \
    $(LIBDIR)/liblist.so \
    $(LIBDIR)/libhasht.so \
    $(LIBDIR)/libpool.so \
    $(LIBDIR)/liblogger.so \
    $(LIBDIR)/libprotocol.so
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SERVEROBJS) -Wl,-rpath=$(LIBDIR) -L $(LIBDIR) $(LIBSERVER)

$(BINDIR)/client: $(CLIENTOBJS) \
    $(LIBDIR)/liblist.so \
    $(LIBDIR)/libclientapi.so \
    $(LIBDIR)/libprotocol.so
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(CLIENTOBJS) -Wl,-rpath=$(LIBDIR) -L $(LIBDIR) $(LIBCLIENT)

# LIBRERIE DINAMICHE

$(LIBDIR)/liblist.so: $(OBJDIR)/list.o $(OBJDIR)/int_list.o
	$(CC) -shared -o $@ $^

$(LIBDIR)/libhasht.so: $(OBJDIR)/hasht.o $(OBJDIR)/conc_hasht.o
	$(CC) -shared -o $@ $^

$(LIBDIR)/libpool.so: $(OBJDIR)/threadpool.o
	$(CC) -shared -o $@ $^

$(LIBDIR)/liblogger.so: $(OBJDIR)/logger.o
	$(CC) -shared -o $@ $^

$(LIBDIR)/libprotocol.so: $(OBJDIR)/protocol.o
	$(CC) -shared -o $@ $^

$(LIBDIR)/libclientapi.so: $(OBJDIR)/client_api.o $(OBJDIR)/filesys_util.o $(OBJDIR)/util.o
	$(CC) -shared -o $@ $^

# DIPENDENZE FILE OGGETTO

$(OBJDIR)/server.o: $(SRCDIR)/server.c \
    $(INCDIR)/config_parser.h \
    $(INCDIR)/eviction_policy.h \
    $(INCDIR)/log_format.h \
    $(INCDIR)/logger.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/storage_server.h \
    $(INCDIR)/threadpool.h \
    $(INCDIR)/util.h

$(OBJDIR)/storage_server.o: $(SRCDIR)/storage_server.c \
    $(INCDIR)/storage_server.h \
    $(INCDIR)/conc_hasht.h \
    $(INCDIR)/config_parser.h \
    $(INCDIR)/eviction_policy.h \
    $(INCDIR)/hasht.h \
    $(INCDIR)/int_list.h \
    $(INCDIR)/list.h \
    $(INCDIR)/log_format.h \
    $(INCDIR)/logger.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/util.h

$(OBJDIR)/eviction_policy.o: $(SRCDIR)/eviction_policy.c \
    $(INCDIR)/eviction_policy.h

$(OBJDIR)/config_parser.o: $(SRCDIR)/config_parser.c \
    $(INCDIR)/config_parser.h \
    $(INCDIR)/eviction_policy.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/util.h

$(OBJDIR)/util.o: $(SRCDIR)/util.c \
    $(INCDIR)/util.h

$(OBJDIR)/client.o: $(SRCDIR)/client.c \
    $(INCDIR)/client_api.h \
    $(INCDIR)/cmdline_operation.h \
    $(INCDIR)/cmdline_parser.h \
    $(INCDIR)/filesys_util.h \
    $(INCDIR)/list.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/util.h

$(OBJDIR)/filesys_util.o: $(SRCDIR)/filesys_util.c \
    $(INCDIR)/filesys_util.h

$(OBJDIR)/cmdline_operation.o: $(SRCDIR)/cmdline_operation.c \
    $(INCDIR)/cmdline_operation.h \
    $(INCDIR)/list.h

$(OBJDIR)/cmdline_parser.o: $(SRCDIR)/cmdline_parser.c \
    $(INCDIR)/cmdline_parser.h \
    $(INCDIR)/client_api.h \
    $(INCDIR)/cmdline_operation.h \
    $(INCDIR)/list.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/util.h

$(OBJDIR)/list.o: $(SRCDIR)/list.c \
    $(INCDIR)/list.h

$(OBJDIR)/int_list.o: $(SRCDIR)/int_list.c \
    $(INCDIR)/list.h \
    $(INCDIR)/int_list.h

$(OBJDIR)/hasht.o: $(SRCDIR)/hasht.c \
    $(INCDIR)/hasht.h

$(OBJDIR)/conc_hasht.o: $(SRCDIR)/conc_hasht.c \
    $(INCDIR)/hasht.h \
    $(INCDIR)/conc_hasht.h

$(OBJDIR)/threadpool.o: $(SRCDIR)/threadpool.c \
    $(INCDIR)/threadpool.h \
    $(INCDIR)/util.h

$(OBJDIR)/logger.o: $(SRCDIR)/logger.c \
    $(INCDIR)/logger.h \
    $(INCDIR)/util.h

$(OBJDIR)/protocol.o: $(SRCDIR)/protocol.c \
    $(INCDIR)/protocol.h

$(OBJDIR)/client_api.o: $(SRCDIR)/client_api.c \
    $(INCDIR)/client_api.h \
    $(INCDIR)/filesys_util.h \
    $(INCDIR)/protocol.h \
    $(INCDIR)/util.h

clean: 
	@rm -f $(TARGETS)

cleanall: clean
	@rm -f $(OBJDIR)/*.o $(LIBDIR)/*.so *~