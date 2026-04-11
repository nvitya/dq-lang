.DEFAULT_GOAL := all

CM_GENERATED_MAKEFILE := Makefile

.PHONY: all autotest test check comptest

all:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) all

autotest test check comptest:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) autotest/fast

%:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) $@
