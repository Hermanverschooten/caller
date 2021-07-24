
PREFIX = $(MIX_APP_PATH)/priv
BUILD = $(MIX_APP_PATH)/obj
CALLER = $(PREFIX)/caller


# Look for the EI library and header files
# For crosscompiled builds, ERL_EI_INCLUDE_DIR and ERL_EI_LIBDIR must be
# passed into the Makefile.
ifeq ($(ERL_EI_INCLUDE_DIR),)
ERL_ROOT_DIR = $(shell erl -eval "io:format(\"~s~n\", [code:root_dir()])" -s init stop -noshell)
ifeq ($(ERL_ROOT_DIR),)
   $(error Could not find the Erlang installation. Check to see that 'erl' is in your PATH)
endif
ERL_EI_INCLUDE_DIR = "$(ERL_ROOT_DIR)/usr/include"
ERL_EI_LIBDIR = "$(ERL_ROOT_DIR)/usr/lib"
endif

# Set Erlang-specific compile and linker flags
ERL_CFLAGS ?= -I$(ERL_EI_INCLUDE_DIR)
ERL_LDFLAGS ?= -L$(ERL_EI_LIBDIR) -lei

LDFLAGS += `pkg-config --cflags --libs libpjproject`

CFLAGS ?= -g -O3
CFLAGS += -pthread -Wall
CFLAGS += -I"$(ERTS_INCLUDE_DIR)"
CFLAGS += -Isrc

CC ?= $(CROSSCOMPILE)-gcc

SRC=src/caller.cpp

.PHONY: all clean install

all: install

install : $(PREFIX) $(BUILD) $(CALLER)

$(PREFIX) $(BUILD):
	mkdir -p $@

$(CALLER): $(SRC)
	$(CC) $(CFLAGS) $(ERL_LDFLAGS) $(LDFLAGS) $^ -o $@

clean:
	$(RM) $(CALLER) $(OBJ)

