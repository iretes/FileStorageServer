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

.PHONY: all test1 test2 generate_test3_files test3 test3_lfu test3_lru test3_lw \
    clean_test clean_test1 clean_test2 clean_test3 clean_tests clean cleanall

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

# TESTS

test1: $(BINDIR)/client $(BINDIR)/server clean_test1
	@echo "Genero alcuni file di test...";\
	for((i=0;i<5;++i)); do\
		head -c $$((1000000*($i+1))) </dev/urandom >test/testfiles/randomfiles/randfile"$$i".dat;\
	done;\
	echo "File di test generati";\
	echo "Avvio il server";\
	valgrind --leak-check=full $(BINDIR)/server -c test/test1/config.txt &>test/test1/output/serverout.txt &\
	SERVER_PID=$$!;\
	echo "Avvio i client";\
	chmod +x test/test1/runclients.sh;\
	./test/test1/runclients.sh;\
	echo "Termino il server...";\
	kill -HUP $$SERVER_PID;\
	wait 2>/dev/null;\
	echo "Server terminato";\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test1/output/log.csv &>test/test1/output/statistics.txt

test2: $(BINDIR)/client $(BINDIR)/server clean_test2
	@echo "Genero alcuni file di test...";\
	for i in {1..5}; do\
		head -c $$((100000*$$i)) </dev/urandom >test/testfiles/randomfiles/randfile"$$i".dat;\
	done;\
	head -c 1000001 </dev/urandom >test/testfiles/randomfiles/randfile6.dat;\
	echo "File di test generati";\
	echo "======= Test della politica di espulsione FIFO =======";\
	echo "Avvio il server";\
	$(BINDIR)/server -c test/test2/fifo/config.txt &>test/test2/fifo/output/serverout.txt &\
	SERVER_PID=$$!;\
	echo "Avvio i client...";\
	chmod +x test/test2/runclients.sh;\
	./test/test2/runclients.sh test/test2/fifo/output FIFO;\
	echo "Termino il server...";\
	kill -HUP $$SERVER_PID;\
	wait 2>/dev/null;\
	echo "Server terminato";\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test2/fifo/output/log.csv &>test/test2/fifo/output/statistics.txt;\
	echo "======= Test della politica di espulsione LFU =======";\
	echo "Avvio il server";\
	$(BINDIR)/server -c test/test2/lfu/config.txt &>test/test2/lfu/output/serverout.txt &\
	SERVER_PID=$$!;\
	echo "Avvio i client...";\
	./test/test2/runclients.sh test/test2/lfu/output LFU;\
	echo "Termino il server...";\
	kill -HUP $$SERVER_PID;\
	wait 2>/dev/null;\
	echo "Server terminato";\
	echo "Calcolo le statistiche";\
	./statistiche.sh test/test2/lfu/output/log.csv &>test/test2/lfu/output/statistics.txt;\
	echo "======= Test della politica di espulsione LRU =======";\
	echo "Avvio il server";\
	$(BINDIR)/server -c test/test2/lru/config.txt &>test/test2/lru/output/serverout.txt &\
	SERVER_PID=$$!;\
	echo "Avvio i client...";\
	./test/test2/runclients.sh test/test2/lru/output LRU;\
	echo "Termino il server...";\
	kill -HUP $$SERVER_PID;\
	wait 2> /dev/null;\
	echo "Server terminato";\
	echo "Calcolo le statistiche";\
	./statistiche.sh test/test2/lru/output/log.csv &>test/test2/lru/output/statistics.txt;\
	echo "======= Test della politica di espulsione LW =======";\
	echo "Avvio il server";\
	$(BINDIR)/server -c test/test2/lw/config.txt &>test/test2/lw/output/serverout.txt &\
	SERVER_PID=$$!;\
	echo "Avvio i client...";\
	./test/test2/runclients.sh test/test2/lw/output LW;\
	echo "Termino il server...";\
	kill -HUP $$SERVER_PID;\
	wait 2>/dev/null;\
	echo "Server terminato";\
	echo "Calcolo le statistiche";\
	./statistiche.sh test/test2/lw/output/log.csv &>test/test2/lw/output/statistics.txt

generate_test3_files: clean_test3
	@echo "Genero alcuni file di test...";\
	for((i=0;i<10;++i)); do\
		if [ ! -d "test/testfiles/randomfiles/"$$i"" ]; then \
			mkdir test/testfiles/randomfiles/"$$i";\
		fi;\
		head -c $$((1000000*1)) </dev/urandom >test/testfiles/randomfiles/"$$i"/randfile1.dat;\
		head -c $$((1000000*5)) </dev/urandom >test/testfiles/randomfiles/"$$i"/randfile2.dat;\
	done;\
	echo "File di test generati"

test3: $(BINDIR)/client $(BINDIR)/server generate_test3_files
	@echo "Avvio il server";\
	$(BINDIR)/server -c test/test3/config_fifo.txt &> test/test3/output/serverout.txt &\
	SERVER_PID=$$!;\
	sleep 30 && kill -INT $$SERVER_PID &\
	chmod +x ./test/test3/runclients.sh;\
	echo "Avvio i client, attendere 30 secondi...";\
	for((i=0;i<10;++i)); do\
		./test/test3/runclients.sh $$i &\
		CLIENTS[i]=$$!;\
	done;\
	wait $$SERVER_PID;\
	echo "Server terminato";\
	for((i=0;i<10;++i)); do\
		kill $${CLIENTS[i]};\
		wait $${CLIENTS[i]} 2>/dev/null;\
	done;\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test3/output/log.csv >test/test3/output/statistics.txt

test3_lfu: $(BINDIR)/client $(BINDIR)/server generate_test3_files
	@echo "Avvio il server";\
	$(BINDIR)/server -c test/test3/config_lfu.txt &> test/test3/output/serverout.txt &\
	SERVER_PID=$$!;\
	sleep 30 && kill -INT $$SERVER_PID &\
	chmod +x ./test/test3/runclients.sh;\
	echo "Avvio i client, attendere 30 secondi...";\
	for((i=0;i<10;++i)); do\
		./test/test3/runclients.sh $$i &\
		CLIENTS[i]=$$!;\
	done;\
	wait $$SERVER_PID;\
	echo "Server terminato";\
	for((i=0;i<10;++i)); do\
		kill $${CLIENTS[i]};\
		wait $${CLIENTS[i]} 2>/dev/null;\
	done;\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test3/output/log.csv >test/test3/output/statistics.txt

test3_lru: $(BINDIR)/client $(BINDIR)/server generate_test3_files
	@echo "Avvio il server";\
	$(BINDIR)/server -c test/test3/config_lru.txt &> test/test3/output/serverout.txt &\
	SERVER_PID=$$!;\
	sleep 30 && kill -INT $$SERVER_PID &\
	chmod +x ./test/test3/runclients.sh;\
	echo "Avvio i client, attendere 30 secondi...";\
	for((i=0;i<10;++i)); do\
		./test/test3/runclients.sh $$i &\
		CLIENTS[i]=$$!;\
	done;\
	wait $$SERVER_PID;\
	echo "Server terminato";\
	for((i=0;i<10;++i)); do\
		kill $${CLIENTS[i]};\
		wait $${CLIENTS[i]} 2>/dev/null;\
	done;\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test3/output/log.csv >test/test3/output/statistics.txt

test3_lw: $(BINDIR)/client $(BINDIR)/server generate_test3_files
	@echo "Avvio il server";\
	$(BINDIR)/server -c test/test3/config_lw.txt &> test/test3/output/serverout.txt &\
	SERVER_PID=$$!;\
	sleep 30 && kill -INT $$SERVER_PID &\
	chmod +x ./test/test3/runclients.sh;\
	echo "Avvio i client, attendere 30 secondi...";\
	for((i=0;i<10;++i)); do\
		./test/test3/runclients.sh $$i &\
		CLIENTS[i]=$$!;\
	done;\
	wait $$SERVER_PID;\
	echo "Server terminato";\
	for((i=0;i<10;++i)); do\
		kill $${CLIENTS[i]};\
		wait $${CLIENTS[i]} 2>/dev/null;\
	done;\
	echo "Calcolo le statistiche";\
	chmod +x statistiche.sh;\
	./statistiche.sh test/test3/output/log.csv >test/test3/output/statistics.txt

# COMANDI PER IL CLEANING

clean_test:
	@rm -f ./log.csv
	@rm -f ./storage_socket

clean_test1: 
	@rm -f -r test/test1/output/*
	@rm -f -r test/testfiles/randomfiles/*

clean_test2: 
	@rm -f -r test/test2/fifo/output/*
	@rm -f -r test/test2/lfu/output/*
	@rm -f -r test/test2/lru/output/*
	@rm -f -r test/test2/lw/output/*
	@rm -f -r test/testfiles/randomfiles/*

clean_test3: 
	@rm -f -r test/test3/output/*
	@rm -f -r test/testfiles/randomfiles/*

clean_tests: clean_test clean_test1 clean_test2 clean_test3

clean: 
	@rm -f $(TARGETS)

cleanall: clean clean_tests
	@rm -f $(OBJDIR)/*.o $(LIBDIR)/*.so *~