
PRODUCTION_PATH ?= /pub/sources/gotcha/production

DEBUG_FLAGS      := -DDEBUG
PRODUCTION_FLAGS :=

BUILD_DIR        := build
BINDIR           := $(BUILD_DIR)/bin
LIBDIR           := $(BUILD_DIR)/lib

NO_WARN_FLAGS := -Wno-pointer-sign
COMMON_CFLAGS := -Wall -g -DLOG_TO_SYSLOG -I$(shell pwd) $(NO_WARN_FLAGS)
APP_LDFLAGS   := -L$(LIBDIR) -lgwebapp

GWEB_LIB  := $(LIBDIR)/libgwebapp.so

GWEB_LIB_SRC := \
    lib/uid.c \
    lib/config.c

GWEB_LIB_CFLAGS := \
    $(COMMON_CFLAGS) -I$(PRODUCTION_PATH)/include \
    $(shell mysql_config --cflags)

GWEB_SERVER_BIN := $(BINDIR)/gwebserver

GWEB_SERVER_SRC := \
    gweb_server.c \
    mysqldb_handler.c \
    json_parser.c \
    avatardb.c

GWEB_SERVER_CFLAGS := \
    $(COMMON_CFLAGS) -I$(PRODUCTION_PATH)/include \
    $(shell mysql_config --cflags)

GWEB_SERVER_LDFLAGS := \
    $(APP_LDFLAGS) \
    -L$(PRODUCTION_PATH)/lib $(shell mysql_config --libs) \
    -lmicrohttpd -ljson-c

MYSQL_SCHEMA_BIN := $(BINDIR)/mysql_schema

MYSQL_SCHEMA_SRC := \
    setup/mysql_schema.c

MYSQL_SCHEMA_CFLAGS := \
    $(COMMON_CFLAGS) -I$(PRODUCTION_PATH)/include \
    $(shell mysql_config --cflags)

MYSQL_SCHEMA_LDFLAGS := \
    $(APP_LDFLAGS) \
    -L$(PRODUCTION_PATH)/lib $(shell mysql_config --libs) \
    -ljson-c

ALL_BINS := $(GWEB_SERVER_BIN) $(MYSQL_SCHEMA_BIN)
ALL_LIBS := $(GWEB_LIB)

.PHONY: build_env_setup

debug: EXTRA_CFLAGS := $(DEBUG_FLAGS)
debug: all

production: EXTRA_CFLAGS := $(PRODUCTION_FLAGS)
production: all

all: build_env_setup $(ALL_LIBS) $(ALL_BINS)

build_env_setup:
	mkdir -p $(BINDIR) $(LIBDIR)

$(GWEB_LIB): $(GWEB_LIB_SRC)
	$(CC) -o $@ $^ -shared -fPIC $(EXTRA_CFLAGS) $(GWEB_LIB_CFLAGS)

$(GWEB_SERVER_BIN): $(GWEB_SERVER_SRC)
	$(CC) -o $@ $^ $(EXTRA_CFLAGS) $(GWEB_SERVER_CFLAGS) $(GWEB_SERVER_LDFLAGS)

$(MYSQL_SCHEMA_BIN): $(MYSQL_SCHEMA_SRC)
	$(CC) -o $@ $^ $(EXTRA_CFLAGS) $(MYSQL_SCHEMA_CFLAGS) $(MYSQL_SCHEMA_LDFLAGS)

clean:
	rm -f *~ *.o $(ALL_BINS) $(ALL_LIBS) *.d

install:
	mkdir -p $(PRODUCTION_PATH)/bin
	cp -f $(ALL_BINS) $(PRODUCTION_PATH)/bin
	cp -f $(ALL_LIBS) $(PRODUCTION_PATH)/lib
