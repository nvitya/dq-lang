.DEFAULT_GOAL := all

CM_GENERATED_MAKEFILE := Makefile

.PHONY: all autotest test check comptest cleantest

all:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) all

autotest:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) autotest/fast

test:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) test/fast

check:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) check/fast

comptest:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) comptest/fast

cleantest:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) cleantest/fast

%:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) $@
