include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

all:

test test-grid:	start-redis-db
	if $(MAKE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-redis-db; $$ok

clean:
	rm -rf failures
	rm -rf cnf/grid

test-impl:
	smartmet-plugin-test \
		--handler /timeseries \
		--reactor-config cnf/reactor.conf
		--timeout 300

start-redis-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi
