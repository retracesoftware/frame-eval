VERSION ?= 3.12.8

SNAPSHOT := build/workflow/sources/$(VERSION)
STAMP := $(SNAPSHOT)/.generated
GENERATOR_INPUTS := regenerate \
	extract.py \
	validate \
	requirements.txt \
	templates/frame_eval.c \
	templates/frame_eval.h \
	templates/retrace_redacted.h \
	tests/exclusions/$(VERSION).txt \
	patches/$(VERSION).patch

.PHONY: all frame-eval-sources regenerate-frame-eval test test-generated
all: frame-eval-sources

frame-eval-sources: $(STAMP)

$(STAMP): $(GENERATOR_INPUTS)
	./regenerate $(VERSION)

regenerate-frame-eval:
	./regenerate $(VERSION)

test:
	python3 -m unittest discover -s tests -v

test-generated:
	./test-generated $(VERSION)
