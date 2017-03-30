
PRODUCTION_PATH ?= /home/rchandramouli/gotcha/production

COMMON_CFLAGS := -I$(shell pwd)

GWEB_SERVER_BIN := bin/gwebserver

GWEB_SERVER_SRC := \
    gweb_server.c \
    mysqldb_handler.c \
    json_parser.c

GWEB_SERVER_CFLAGS := \
    $(COMMON_CFLAGS) -I$(PRODUCTION_PATH)/include \
    $(shell mysql_config --cflags)

GWEB_SERVER_LDFLAGS := \
    -L$(PRODUCTION_PATH)/lib $(shell mysql_config --libs) \
    -lmicrohttpd -ljson-c

MYSQL_SCHEMA_BIN := bin/mysql_schema

MYSQL_SCHEMA_SRC := \
    setup/mysql_schema.c

MYSQL_SCHEMA_CFLAGS := \
    $(COMMON_CFLAGS) -I$(PRODUCTION_PATH)/include \
    $(shell mysql_config --cflags)

MYSQL_SCHEMA_LDFLAGS := $(shell mysql_config --libs)

ALL_BINS := $(GWEB_SERVER_BIN) $(MYSQL_SCHEMA_BIN)

all: $(ALL_BINS)

$(GWEB_SERVER_BIN): $(GWEB_SERVER_SRC)
	$(CC) -o $@ $^ $(GWEB_SERVER_CFLAGS) $(GWEB_SERVER_LDFLAGS)

$(MYSQL_SCHEMA_BIN): $(MYSQL_SCHEMA_SRC)
	$(CC) -o $@ $^ $(MYSQL_SCHEMA_CFLAGS) $(MYSQL_SCHEMA_LDFLAGS)

clean:
	rm -f *~ *.o $(ALL_BINS) *.d

install:
	mkdir -p $(PRODUCTION_PATH)/bin
	cp -f $(ALL_BINS) $(PRODUCTION_PATH)/bin
