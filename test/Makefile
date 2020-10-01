PROG = $(patsubst %.cpp,%,$(wildcard *Test*.cpp))

REQUIRES_GDAL = yes

include ../common.mk

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

# Unfortunately otlv4.h forces us to define the full oracle include path
INCLUDES += \
	-I ../timeseries \
	-isystem /usr/include/mysql \
	-I /usr/include/smartmet \
	-I /usr/include/smartmet/grid-files \
	-I /usr/include/smartmet/grid-content \
	-isystem /usr/include/soci \
	-isystem /usr/include/oracle/11.2/client64 \
	`pkg-config --cflags libconfig++`

LIBS +=  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lsmartmet-grid-files \
	-lsmartmet-grid-content \
	`pkg-config --libs libconfig++` \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_system \
	-lpthread -ldl

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
	@echo 
	@echo "*************************************************************"
	@echo "*********** Running timeseries querydata tests **************"
	@echo "*************************************************************"
	@echo 
	@./PluginTest cnf/reactor.conf

grid-test: $(PROG) $(CONFIG_FILES)
	@mkdir -p failures-grid
	@rm -f failures-grid/*
	@echo 
	@echo "*************************************************************"
	@echo "*********** Running timeseries grid tests *******************"
	@echo "*************************************************************"
	@echo 
	@echo "*** Starting the Redis database."
	@redis-server cnf/redis.conf
	@echo "*** Executing plugin tests."
	@echo 
	@./PluginTestGrid cnf/reactor-grid.conf
	@echo 
	@echo "*** Shutting down the Redis database and removing useless files." 
	@echo 
	@cat cnf/grid/redis/redis-server.pid | xargs kill -9
	@rm -f cnf/grid/redis/redis-server.pid
	@rm -f cnf/grid/redis/redis-server.log

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
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
