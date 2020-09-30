# User specific settings

-include $(HOME)/.smartmet.mk

# Installation

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

processor := $(shell uname -p)
ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
datadir = $(PREFIX)/share
objdir = obj

enginedir = $(datadir)/smartmet/engines
plugindir = $(datadir)/smartmet/plugins

# Compiler flags

GCC_DIAG_COLOR ?= always

ifneq ($(findstring clang,$(CXX)),)
  USE_CLANG=yes
  CXX_STD ?= c++17
else
  USE_CLANG=no
  CXX_STD ?= c++11
endif

FLAGS = -std=$(CXX_STD) -fdiagnostics-color=$(GCC_DIAG_COLOR) \
	-ggdb3 -fPIC -fno-omit-frame-pointer \
	-Wall -Wextra \
	-Wno-unused-parameter

FLAGS_DEBUG = -Og -Werror -Wpedantic -Wundef
FLAGS_RELEASE = -O2 -Wuninitialized

ifeq ($(USE_CLANG), yes)
  FLAGS_DEBUG += -Wshadow -Wweak-vtables -Wzero-as-null-pointer-constant
  # clang does not by default provide parameter --build-id to linker. Add it directly
  LDFLAGS += -Wl,--build-id=sha1
endif

# Sanitizer support

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract \
           -fsanitize=undefined -fsanitize-address-use-after-scope
endif

# Compile modes (debug / release)

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  CFLAGS = $(DEFINES) $(FLAGS) $(FLAGS_DEBUG)
else
  CFLAGS = $(DEFINES) $(FLAGS) $(FLAGS_RELEASE)
endif

# Include paths and libs

ifneq ($(PREFIX),/usr)
  INCLUDES += -isystem $(includedir)
endif

INCLUDES += -I$(includedir)/smartmet

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

ifeq ($(REQUIRES_GDAL),yes)
  ifneq "$(wildcard /usr/gdal30/include)" ""
    INCLUDES += -isystem /usr/gdal30/include
    LIBS += -L$(PREFIX)/gdal30/lib
  else
    INCLUDES += -isystem /usr/include/gdal
  endif
endif

ifeq ($(REQUIRES_PGSQL),yes)
  ifneq "$(wildcard /usr/pgsql-12/lib)" ""
    LIBS += -L$(PREFIX)/pgsql12-lib -l:libpq.so.5
  else
    LIBS += -L$(PREFIX)/pgsql-9.5/lib -l:libpq.so.5
  endif
endif


