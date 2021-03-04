PROG = PluginTest TestConfigCreator

REQUIRES = gdal configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

# Unfortunately otlv4.h forces us to define the full oracle include path
INCLUDES += \
	-I ../timeseries \
	-isystem /usr/include/mysql \
	-I /usr/include/smartmet \
	-I /usr/include/smartmet/grid-files \
	-I /usr/include/smartmet/grid-content \
	-isystem /usr/include/soci \
	-isystem /usr/include/oracle/11.2/client64

LIBS +=  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lsmartmet-grid-files \
	-lsmartmet-grid-content \
	$(CONFIGPP_LIBS) \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_system \
	-lpthread -ldl

all: $(PROG)

clean:
	rm -f $(PROG) *~
	rm -rf failures

test: $(PROG) $(CONFIG_FILES)
	@mkdir -p failures
	@rm -f failures/*
	@echo 
	@echo "*************************************************************"
	@echo "*********** Running timeseries grid tests *******************"
	@echo "*************************************************************"
	@echo 
	@echo "*** Creating the Redis configuration file."
	@./TestConfigCreator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf
	@echo "*** Executing plugin tests."
	@echo 
	@./PluginTest -c cnf/reactor.conf
	@rm -f cnf/grid/redis/redis.conf
#	@echo 
#	@echo "*** Shutting down the Redis database and removing useless files." 
#	@echo 
#	@cat cnf/grid/redis/redis-server.pid | xargs kill -9
#	@rm -f cnf/grid/redis/redis-server.pid
#	@rm -f cnf/grid/redis/redis-server.log

$(PROG) : % : %.cpp Makefile ../gribtimeseries.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
