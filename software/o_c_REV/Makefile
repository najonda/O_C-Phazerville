ifdef VERBOSE
AT=
PIO_OPTS+=
else
AT=@
PIO_OPTS+=-s
endif

.PHONY: all
all: upload

.PHONY: dev
dev:
	$(AT)pio run $(PIO_OPTS) -e oc_dev

.PHONY: upload
upload:
	$(AT)pio run $(PIO_OPTS) --target upload -e oc_dev

.PHONY: clean
clean:
	$(AT)pio run $(PIO_OPTS) --target clean -e oc_dev

.PHONY: prod
prod:
	$(AT)pio run $(PIO_OPTS) -e oc_prod
