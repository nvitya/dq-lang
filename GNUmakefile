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

.PHONY: cross-aarch64-linux cross-armhf-linux cross-rv64g-linux cross-x86_64-win

cross-aarch64-linux:
	cmake -S . -B build-cross/aarch64-linux -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-linux.cmake $(CMAKE_EXTRA_ARGS)
	$(MAKE) -j"$$(nproc)" -C build-cross/aarch64-linux

cross-armhf-linux:
	cmake -S . -B build-cross/armhf-linux -DCMAKE_TOOLCHAIN_FILE=toolchains/armhf-linux.cmake $(CMAKE_EXTRA_ARGS)
	$(MAKE) -j"$$(nproc)" -C build-cross/armhf-linux

cross-rv64g-linux:
	cmake -S . -B build-cross/rv64g-linux -DCMAKE_TOOLCHAIN_FILE=toolchains/rv64g-linux.cmake $(CMAKE_EXTRA_ARGS)
	$(MAKE) -j"$$(nproc)" -C build-cross/rv64g-linux

cross-x86_64-win:
	cmake -S . -B build-cross/x86_64-win -DCMAKE_TOOLCHAIN_FILE=toolchains/x86_64-win.cmake $(CMAKE_EXTRA_ARGS)
	$(MAKE) -j"$$(nproc)" -C build-cross/x86_64-win


%:
	@$(MAKE) --no-print-directory -f $(CM_GENERATED_MAKEFILE) $@
