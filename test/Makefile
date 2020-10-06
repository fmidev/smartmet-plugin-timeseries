PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES_GDAL = yes

include ../common.mk

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

INCLUDES += \
	-I ../timeseries \
	-I /usr/include/smartmet \
	-isystem /usr/gdal30/include \
	-isystem /usr/include/mysql \
	-isystem /usr/include/soci \
	-isystem /usr/include/oracle/11.2/client64 \
	`pkg-config --cflags libconfig++`

LIBS +=  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	`pkg-config --libs libconfig++` \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_system \
	-L/usr/gdal30/lib -lgdal \
	-lbz2 -lz -lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG)
	@mkdir -p failures
	@rm -f failures/*
	@echo Running tests:
	@rm -f *.err
	@for prog in $(PROG); do \
		( ./$$prog || touch $$prog.err ) ; \
	 done
	@test `find . -name \*.err | wc -l` = "0" || ( echo ; echo "The following tests have errors:" ; \
	   for i in *.err ; do echo `basename $$i .err`; done ; rm -f *.err ; false ) 

$(PROG) : % : %.cpp Makefile ../timeseries.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
