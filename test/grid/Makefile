include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

all:

clean:
	rm -rf failures

test:
	smartmet-plugin-test \
		--handler /timeseries \
		--reactor-config cnf/reactor.conf
		--timeout 300
