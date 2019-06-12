PROG = $(patsubst %.cpp,%,$(wildcard *Test*.cpp))

MAINFLAGS = -std=c++0x -Wall -W -Wno-unused-parameter

ENV_FILE = cnf/smartmet-test-env.cfg

CONFIG_FILES = \
	cnf/gis.conf \
	cnf/geonames.conf \
	cnf/querydata.conf \
	cnf/observation.conf \
	cnf/timeseries.conf \
	cnf/timeseries-grid.conf \
	cnf/redis.conf


EXTRAFLAGS = -Werror -pedantic -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Winline \
	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wno-pmf-conversions \
	-Wsign-promo -Wchar-subscripts -Wold-style-cast \
	-Wshadow -Woverloaded-virtual

DIFFICULTFLAGS = -Weffc++ -Wunreachable-code -Wredundant-decls

CC = g++
CFLAGS = -DUNIX -O0 -g $(MAINFLAGS) -Wno-unknown-pragmas

# Unfortunately otlv4.h forces us to define the full oracle include path
INCLUDES = \
	-I ../timeseries \
	-I /usr/include/mysql \
	-I /usr/include/smartmet \
  -I /usr/include/smartmet/grid-files \
  -I /usr/include/smartmet/grid-content \
	-I /usr/include/soci \
	-I/usr/include/oracle/11.2/client64 \
	`pkg-config --cflags libconfig++`

LIBS =  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lsmartmet-grid-files \
	-lsmartmet-grid-content \
	`pkg-config --libs libconfig++` \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_system \
	-lbz2 -lz -lpthread -ldl

all: $(PROG)

clean:
	rm -f $(PROG) *~
	rm -f cnf/gis.conf cnf/geonames.conf cnf/querydata.conf cnf/observation.conf cnf/timeseries.conf cnf/timeseries-grid.conf cnf/redis.conf
	rm -rf failures
	rm -rf failures-grid

test: 
	@make grid-test
	@make qd-test

qd-test: $(PROG) $(CONFIG_FILES)
	@mkdir -p failures
	@rm -f failures/*
	@rm -rf failures-qd
	@echo 
	@echo "*************************************************************"
	@echo "*********** Running timeseries querydata tests **************"
	@echo "*************************************************************"
	@echo 
	@./PluginTest cnf/reactor.conf
	@mv failures failures-qd

grid-test: $(PROG) $(CONFIG_FILES)
	@mkdir -p failures
	@rm -f failures/*
	@rm -rf failures-grid
	@mv output output-qd
	@mv output-grid output
	@echo 
	@echo "*************************************************************"
	@echo "*********** Running timeseries grid tests *******************"
	@echo "*************************************************************"
	@echo 
	@echo "*** Starting the Redis database."
	@redis-server cnf/redis.conf
	@echo "*** Executing plugin tests."
	@echo 
	@./PluginTest cnf/reactor-grid.conf
	@echo 
	@echo "*** Shutting down the Redis database and removing useless files." 
	@echo 
	@cat cnf/grid/redis/redis-server.pid | xargs kill -9
	@rm -f cnf/grid/redis/redis-server.pid
	@rm -f cnf/grid/redis/redis-server.log
	@mv failures failures-grid
	@mv output output-grid
	@mv output-qd output


config: $(CONFIG_FILES)

clear-config:
	rm -f  cnf/gis.conf cnf/geonames.conf cnf/querydata.conf cnf/observation.conf cnf/timeseries.conf cnf/timeseries-grid.conf cnf/redis.conf

cnf/gis.conf: cnf/templates/gis.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the gis engine." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/gis.conf cnf/gis.conf

cnf/geonames.conf: cnf/templates/geonames.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the geonames engine." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/geonames.conf cnf/geonames.conf

cnf/querydata.conf: cnf/templates/querydata.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the querydata engine." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/querydata.conf cnf/querydata.conf

cnf/observation.conf: cnf/templates/observation.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the observation engine." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/observation.conf cnf/observation.conf

cnf/timeseries.conf: cnf/templates/timeseries.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the timeseries plugin with querydata support." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/timeseries.conf cnf/timeseries.conf -D PRIMARY_FORECAST_SOURCE querydata -D GRID_ENGINE_DISABLED true

cnf/timeseries-grid.conf: cnf/templates/timeseries.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the timeseries plugin with grib support." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/timeseries.conf cnf/timeseries-grid.conf -D PRIMARY_FORECAST_SOURCE grid -D GRID_ENGINE_DISABLED false

cnf/redis.conf: cnf/templates/redis.conf $(ENV_FILE)
	@echo "*** Creating configuration file for the redis database." 
	@./TestConfigCreator $(ENV_FILE) cnf/templates/redis.conf cnf/redis.conf

$(PROG) : % : %.cpp Makefile ../timeseries.so
	$(CC) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
