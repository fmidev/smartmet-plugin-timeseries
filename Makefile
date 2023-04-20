SUBNAME = timeseries
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

REQUIRES = gdal configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# Compiler options

CFLAGS += -Wno-maybe-uninitialized -Wno-pedantic

DEFINES = -DUNIX -D_REENTRANT

INCLUDES += -isystem $(includedir)/mysql \
	-isystem $(includedir)/soci \
	-isystem $(includedir)/oracle/11.2/client64

LIBS += -L$(libdir) \
	-lsmartmet-timeseries \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-locus \
        -lsmartmet-gis \
	-lsmartmet-macgyver \
	-lsmartmet-grid-content \
	-lboost_date_time \
	-lboost_thread \
	-lboost_iostreams \
	$(REQUIRED_LIBS) \
	-lbz2 -lz

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test test-grid rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol | grep -v SmartMet::Engine:: |\
                        grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

clean:
	rm -f $(LIBFILE) *~ $(SUBNAME)/*~
	rm -rf obj
	$(MAKE) -C test $@

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp

install:
	@mkdir -p $(plugindir)
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)

test:
	$(MAKE) -C test test

test-sqlite test-oracle test-postgresql test-grid:
	$(MAKE) -C test $@

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude test-grid --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

-include $(wildcard obj/*.d)
