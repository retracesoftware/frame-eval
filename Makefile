VERSION ?= 3.12.8

SNAPSHOT := build/workflow/sources/$(VERSION)
STAMP := $(SNAPSHOT)/.generated
GENERATOR_INPUTS := regenerate \
	extract.py \
	validate \
	requirements.txt \
	templates/retrace_redacted.h \
	patches/$(VERSION).patch

.PHONY: all frame-eval-sources regenerate-frame-eval test
all: frame-eval-sources

frame-eval-sources: $(STAMP)

$(STAMP): $(GENERATOR_INPUTS)
	./regenerate $(VERSION)

regenerate-frame-eval:
	./regenerate $(VERSION)

test:
	python3 -m unittest discover -s tests -v
