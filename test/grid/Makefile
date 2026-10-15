include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_TIMEOUT ?= 300

all:

test test-grid:	start-valkey-db
	if $(MAKE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-valkey-db; $$ok

clean:
	rm -rf failures
	rm -rf cnf/grid

test-impl:
	$(TEST_RUNNER) smartmet-plugin-test \
		--handler /timeseries \
		--reactor-config cnf/reactor.conf \
		--timeout $(TEST_TIMEOUT)

start-valkey-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@valkey-server cnf/grid/redis/redis.conf & echo $$! > cnf/grid/redis/valkey-server.pid

stop-valkey-db:
	@if [ -f cnf/grid/redis/valkey-server.pid ] ; then \
		echo "*** Stopping the Valkey database." ; \
		cat cnf/grid/redis/valkey-server.pid | xargs kill -9 2>/dev/null; \
		rm -f cnf/grid/redis/valkey-server.pid; \
		true; \
	fi
