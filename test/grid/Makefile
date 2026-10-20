include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_TIMEOUT ?= 300

# Starts the Redis test database listening only on a Unix socket below /tmp
GRID_TEST_REDIS ?= smartmet-grid-test-redis

all:

test test-grid:	start-valkey-db
	if $(MAKE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-valkey-db; $$ok

clean:
	rm -rf failures
	-$(GRID_TEST_REDIS) stop
	rm -rf cnf/grid

test-impl:
	$(TEST_RUNNER) smartmet-plugin-test \
		--handler /timeseries \
		--reactor-config cnf/reactor.conf \
		--timeout $(TEST_TIMEOUT)

start-valkey-db:
	$(GRID_TEST_REDIS) start

stop-valkey-db:
	$(GRID_TEST_REDIS) stop
