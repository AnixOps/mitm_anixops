CC ?= cc
AR ?= ar

BUILD_DIR ?= build
INCLUDE_DIR := include
SRC := src/mitm_anixops.c
OBJ := $(BUILD_DIR)/mitm_anixops.o
STATIC_LIB := $(BUILD_DIR)/libmitm_anixops.a
SHARED_LIB := $(BUILD_DIR)/libmitm_anixops.so
TEST_BIN := $(BUILD_DIR)/test_link
TEST_SRCS := $(filter-out tests/pkg_config_smoke.c,$(sort $(wildcard tests/*.c)))
DEMO_BIN := $(BUILD_DIR)/strategy_chain_demo
RUNNER_BIN := $(BUILD_DIR)/anixops-mitm-runner
PROXY_SHIM_BIN := $(BUILD_DIR)/anixops-mitm-proxy-shim
SCRIPT_RUNNER_JS := e2e/script_runtime/anixops_runner.js
PKG_CONFIG_FILE := pkgconfig/mitm_anixops.pc
CMAKE_CONFIG_FILES := cmake/mitm_anixops-config.cmake cmake/mitm_anixops-config-version.cmake
ALPHA_NAME ?= anixops-mitm-alpha-0.45.6

CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -fPIC
CPPFLAGS ?= -I$(INCLUDE_DIR)
LDFLAGS_SHARED ?= -shared
LDLIBS ?=

ifeq ($(PCRE2),1)
CPPFLAGS += -DANIXOPS_ENABLE_PCRE2
LDLIBS += -lpcre2-8
endif

ifeq ($(JQ),1)
CPPFLAGS += -DANIXOPS_ENABLE_LIBJQ
LDLIBS += -ljq
endif

.PHONY: all clean test pcre2-test jq-test demo demo-check runner runner-check proxy-shim proxy-shim-check pkg-config-check go-binding-check rust-binding-check cmake-package-check alpha-dist ordering-contract-check e2e bili-e2e script-contract-e2e bilibili-homepage-demo-e2e e2e-all

all: $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ): $(SRC) $(INCLUDE_DIR)/mitm_anixops.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $(STATIC_LIB) $(OBJ)

$(SHARED_LIB): $(OBJ)
	$(CC) $(LDFLAGS_SHARED) -o $(SHARED_LIB) $(OBJ) $(LDLIBS)

$(TEST_BIN): $(TEST_SRCS) $(STATIC_LIB) $(INCLUDE_DIR)/mitm_anixops.h tests/test_harness.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror $(TEST_SRCS) $(STATIC_LIB) -o $(TEST_BIN) $(LDLIBS)

test: $(TEST_BIN)
	$(TEST_BIN)

pcre2-test:
	$(MAKE) PCRE2=1 BUILD_DIR=build-pcre2 clean test

jq-test:
	$(MAKE) JQ=1 BUILD_DIR=build-jq clean test

$(DEMO_BIN): examples/strategy_chain_demo.c $(STATIC_LIB) $(INCLUDE_DIR)/mitm_anixops.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror examples/strategy_chain_demo.c $(STATIC_LIB) -o $(DEMO_BIN) $(LDLIBS)

demo: $(DEMO_BIN)
	$(DEMO_BIN)

demo-check: $(DEMO_BIN)
	$(DEMO_BIN) | tee $(BUILD_DIR)/strategy_chain_demo.out
	grep -F 'mitm tcp decision=1 reason=7 pattern=api.example.com' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'mitm quic decision=2 reason=6 pattern=api.example.com' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'rewrite url action=1 status=302 value=https://new.example/path' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'rewrite response-body action=12 body=clean=1&ok=1' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'script kind=2 requires_body=1 path=https://scripts.example/demo.js tag=demo.response argument=Mode=demo' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null

$(RUNNER_BIN): tools/anixops_mitm_runner.c $(STATIC_LIB) $(INCLUDE_DIR)/mitm_anixops.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror tools/anixops_mitm_runner.c $(STATIC_LIB) -o $(RUNNER_BIN) $(LDLIBS)

runner: $(RUNNER_BIN)

runner-check: $(RUNNER_BIN)
	$(RUNNER_BIN) scan tests/fixtures/Representative.Loon.plugin > $(BUILD_DIR)/runner-scan.out
	grep -F '"status":0' $(BUILD_DIR)/runner-scan.out >/dev/null
	grep -F '"counts":{"mitm":2,"rewrite":2,"script":2,"argument":1}' $(BUILD_DIR)/runner-scan.out >/dev/null
	$(RUNNER_BIN) scan --corpus tests/fixtures/corpus/manifest.json > $(BUILD_DIR)/runner-corpus.out
	node -e "JSON.parse(require('fs').readFileSync(process.argv[1], 'utf8'))" $(BUILD_DIR)/runner-corpus.out
	grep -F '"fixtureCount":4' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"sha256Matched":true' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"expectedDiagnostics":{"accepted":16,"ignored":7,"rejected":0}' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"platform":"loon"' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"platform":"surge"' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"platform":"quantumultx"' $(BUILD_DIR)/runner-corpus.out >/dev/null
	grep -F '"passed":true' $(BUILD_DIR)/runner-corpus.out >/dev/null
	$(RUNNER_BIN) trace --plugin tests/fixtures/Representative.Loon.plugin --url https://api.loon.example/v1 --phase response > $(BUILD_DIR)/runner-trace.out
	grep -F '"kind":2' $(BUILD_DIR)/runner-trace.out >/dev/null
	grep -F '"tag":"loon.response"' $(BUILD_DIR)/runner-trace.out >/dev/null
	$(RUNNER_BIN) trace --plugin tests/fixtures/Representative.Surge.sgmodule --url https://api.surge.example/v1 --phase response > $(BUILD_DIR)/runner-surge-trace.out
	grep -F '"timeoutMs":1250' $(BUILD_DIR)/runner-surge-trace.out >/dev/null
	grep -F '"maxSize":4096' $(BUILD_DIR)/runner-surge-trace.out >/dev/null
	$(RUNNER_BIN) replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv > $(BUILD_DIR)/runner-replay.out
	grep -F '"name":"redirect"' $(BUILD_DIR)/runner-replay.out >/dev/null
	grep -F '"value":"https://api.loon.example/v1"' $(BUILD_DIR)/runner-replay.out >/dev/null
	grep -F '"name":"reject-ads"' $(BUILD_DIR)/runner-replay.out >/dev/null
	grep -F '"action":4' $(BUILD_DIR)/runner-replay.out >/dev/null
	grep -F '"tag":"loon.request"' $(BUILD_DIR)/runner-replay.out >/dev/null
	grep -F '"tag":"loon.response"' $(BUILD_DIR)/runner-replay.out >/dev/null
	rm -f $(BUILD_DIR)/runner-script-store.json
	$(RUNNER_BIN) replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv --script-runner e2e/script_runtime/anixops_runner.js --script-store $(BUILD_DIR)/runner-script-store.json --script-map https://scripts.example/loon-request.js=tests/fixtures/runner_replay_script.js --script-map https://scripts.example/loon-response.js=tests/fixtures/runner_replay_script.js > $(BUILD_DIR)/runner-replay-runtime.out
	grep -F '"message":"script executed"' $(BUILD_DIR)/runner-replay-runtime.out >/dev/null
	grep -F 'requestRuntime' $(BUILD_DIR)/runner-replay-runtime.out >/dev/null
	grep -F 'responseRuntime' $(BUILD_DIR)/runner-replay-runtime.out >/dev/null
	grep -F 'storeAfter' $(BUILD_DIR)/runner-replay-runtime.out >/dev/null
	grep -F 'storeCount' $(BUILD_DIR)/runner-replay-runtime.out >/dev/null
	grep -F '"runner.count": "1"' $(BUILD_DIR)/runner-script-store.json >/dev/null
	rm -f $(BUILD_DIR)/runner-script-bundle-store.json
	$(RUNNER_BIN) replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv --script-runner e2e/script_runtime/anixops_runner.js --script-store $(BUILD_DIR)/runner-script-bundle-store.json --script-bundle tests/fixtures/ScriptBundle.json > $(BUILD_DIR)/runner-replay-script-bundle.out
	grep -F '"message":"script executed"' $(BUILD_DIR)/runner-replay-script-bundle.out >/dev/null
	grep -F '"sha256Matched":true' $(BUILD_DIR)/runner-replay-script-bundle.out >/dev/null
	$(RUNNER_BIN) replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv --script-runner e2e/script_runtime/anixops_runner.js --script-bundle tests/fixtures/ScriptBundleBadDigest.json > $(BUILD_DIR)/runner-replay-script-bad-digest.out
	grep -F '"message":"script digest mismatch"' $(BUILD_DIR)/runner-replay-script-bad-digest.out >/dev/null
	grep -F '"sha256Matched":false' $(BUILD_DIR)/runner-replay-script-bad-digest.out >/dev/null
	$(RUNNER_BIN) replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv --script-runner e2e/script_runtime/anixops_runner.js --script-bundle tests/fixtures/ScriptBundlePartial.json > $(BUILD_DIR)/runner-replay-script-cache-miss.out
	grep -F '"message":"script cache miss"' $(BUILD_DIR)/runner-replay-script-cache-miss.out >/dev/null

$(PROXY_SHIM_BIN): $(STATIC_LIB) e2e/shim/main.go | $(BUILD_DIR)
	cd e2e/shim && GO111MODULE=off go build -o ../../$(PROXY_SHIM_BIN) .

proxy-shim: $(PROXY_SHIM_BIN)

proxy-shim-check: $(PROXY_SHIM_BIN)
	test -x $(PROXY_SHIM_BIN)

pkg-config-check: all $(PKG_CONFIG_FILE)
	rm -rf $(BUILD_DIR)/pkgconfig-check
	mkdir -p $(BUILD_DIR)/pkgconfig-check/include $(BUILD_DIR)/pkgconfig-check/lib/pkgconfig
	cp include/mitm_anixops.h $(BUILD_DIR)/pkgconfig-check/include/
	cp $(STATIC_LIB) $(SHARED_LIB) $(BUILD_DIR)/pkgconfig-check/lib/
	cp $(PKG_CONFIG_FILE) $(BUILD_DIR)/pkgconfig-check/lib/pkgconfig/
	PKG_CONFIG_PATH="$(abspath $(BUILD_DIR)/pkgconfig-check/lib/pkgconfig)" $(CC) -std=c99 -Wall -Wextra -Werror tests/pkg_config_smoke.c -o $(BUILD_DIR)/pkg-config-smoke $$(PKG_CONFIG_PATH="$(abspath $(BUILD_DIR)/pkgconfig-check/lib/pkgconfig)" pkg-config --cflags --libs mitm_anixops)
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR)/pkgconfig-check/lib)" $(BUILD_DIR)/pkg-config-smoke

go-binding-check: all $(PKG_CONFIG_FILE)
	rm -rf $(BUILD_DIR)/go-binding-check
	mkdir -p $(BUILD_DIR)/go-binding-check/include $(BUILD_DIR)/go-binding-check/lib/pkgconfig
	cp include/mitm_anixops.h $(BUILD_DIR)/go-binding-check/include/
	cp $(STATIC_LIB) $(SHARED_LIB) $(BUILD_DIR)/go-binding-check/lib/
	cp $(PKG_CONFIG_FILE) $(BUILD_DIR)/go-binding-check/lib/pkgconfig/
	PKG_CONFIG_PATH="$(abspath $(BUILD_DIR)/go-binding-check/lib/pkgconfig)" LD_LIBRARY_PATH="$(abspath $(BUILD_DIR)/go-binding-check/lib)" GO111MODULE=off CGO_ENABLED=1 go test ./bindings/go/anixops

rust-binding-check: all $(PKG_CONFIG_FILE)
	rm -rf $(BUILD_DIR)/rust-binding-check
	mkdir -p $(BUILD_DIR)/rust-binding-check/include $(BUILD_DIR)/rust-binding-check/lib/pkgconfig
	cp include/mitm_anixops.h $(BUILD_DIR)/rust-binding-check/include/
	cp $(STATIC_LIB) $(SHARED_LIB) $(BUILD_DIR)/rust-binding-check/lib/
	cp $(PKG_CONFIG_FILE) $(BUILD_DIR)/rust-binding-check/lib/pkgconfig/
	PKG_CONFIG_PATH="$(abspath $(BUILD_DIR)/rust-binding-check/lib/pkgconfig)" LD_LIBRARY_PATH="$(abspath $(BUILD_DIR)/rust-binding-check/lib)" CARGO_TARGET_DIR="$(abspath $(BUILD_DIR)/rust-target)" cargo test --manifest-path bindings/rust/mitm-anixops/Cargo.toml

cmake-package-check: all $(CMAKE_CONFIG_FILES)
	rm -rf $(BUILD_DIR)/cmake-package-check
	mkdir -p $(BUILD_DIR)/cmake-package-check/include $(BUILD_DIR)/cmake-package-check/lib/cmake/mitm_anixops
	cp include/mitm_anixops.h $(BUILD_DIR)/cmake-package-check/include/
	cp $(STATIC_LIB) $(SHARED_LIB) $(BUILD_DIR)/cmake-package-check/lib/
	cp $(CMAKE_CONFIG_FILES) $(BUILD_DIR)/cmake-package-check/lib/cmake/mitm_anixops/
	grep -F 'mitm_anixops::mitm_anixops' $(BUILD_DIR)/cmake-package-check/lib/cmake/mitm_anixops/mitm_anixops-config.cmake >/dev/null
	grep -F 'PACKAGE_VERSION "0.45.6"' $(BUILD_DIR)/cmake-package-check/lib/cmake/mitm_anixops/mitm_anixops-config-version.cmake >/dev/null
	if command -v cmake >/dev/null 2>&1; then \
		cmake -S tests/cmake_smoke -B $(BUILD_DIR)/cmake-smoke -DCMAKE_PREFIX_PATH="$(abspath $(BUILD_DIR)/cmake-package-check)" >/dev/null; \
		cmake --build $(BUILD_DIR)/cmake-smoke >/dev/null; \
		LD_LIBRARY_PATH="$(abspath $(BUILD_DIR)/cmake-package-check/lib)" $(BUILD_DIR)/cmake-smoke/mitm_anixops_cmake_smoke; \
	else \
		printf '%s\n' 'skip cmake configure/build smoke: cmake not found'; \
	fi

alpha-dist: all runner proxy-shim
	rm -rf $(BUILD_DIR)/$(ALPHA_NAME)
	mkdir -p $(BUILD_DIR)/$(ALPHA_NAME)/include $(BUILD_DIR)/$(ALPHA_NAME)/lib/pkgconfig $(BUILD_DIR)/$(ALPHA_NAME)/lib/cmake/mitm_anixops $(BUILD_DIR)/$(ALPHA_NAME)/bin $(BUILD_DIR)/$(ALPHA_NAME)/docs $(BUILD_DIR)/$(ALPHA_NAME)/fixtures/corpus $(BUILD_DIR)/$(ALPHA_NAME)/bindings/go/anixops $(BUILD_DIR)/$(ALPHA_NAME)/bindings/rust/mitm-anixops/src
	cp include/mitm_anixops.h $(BUILD_DIR)/$(ALPHA_NAME)/include/
	cp $(STATIC_LIB) $(SHARED_LIB) $(BUILD_DIR)/$(ALPHA_NAME)/lib/
	cp $(PKG_CONFIG_FILE) $(BUILD_DIR)/$(ALPHA_NAME)/lib/pkgconfig/
	cp $(CMAKE_CONFIG_FILES) $(BUILD_DIR)/$(ALPHA_NAME)/lib/cmake/mitm_anixops/
	cp $(RUNNER_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/
	cp $(PROXY_SHIM_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/
	cp $(SCRIPT_RUNNER_JS) $(BUILD_DIR)/$(ALPHA_NAME)/bin/anixops-script-runner.js
	cp bindings/go/anixops/*.go $(BUILD_DIR)/$(ALPHA_NAME)/bindings/go/anixops/
	cp bindings/rust/mitm-anixops/Cargo.toml bindings/rust/mitm-anixops/Cargo.lock bindings/rust/mitm-anixops/build.rs $(BUILD_DIR)/$(ALPHA_NAME)/bindings/rust/mitm-anixops/
	cp bindings/rust/mitm-anixops/src/*.rs $(BUILD_DIR)/$(ALPHA_NAME)/bindings/rust/mitm-anixops/src/
	cp tests/fixtures/BiliBili.Enhanced.plugin tests/fixtures/Representative.Loon.plugin tests/fixtures/Representative.Surge.sgmodule tests/fixtures/Representative.QuantumultX.snippet tests/fixtures/RunnerReplay.tsv tests/fixtures/runner_replay_script.js tests/fixtures/ScriptBundle.json tests/fixtures/ScriptBundleBadDigest.json tests/fixtures/ScriptBundlePartial.json $(BUILD_DIR)/$(ALPHA_NAME)/fixtures/
	cp tests/fixtures/corpus/manifest.json $(BUILD_DIR)/$(ALPHA_NAME)/fixtures/corpus/
	cp README.md docs/TODO.md docs/compatibility_matrix.md docs/alpha_release_notes.md docs/loon_mitm_full_compat_plan.md docs/loon_mitm_full_compat_brief_zh.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/
	tar -C $(BUILD_DIR) -czf $(BUILD_DIR)/$(ALPHA_NAME).tar.gz $(ALPHA_NAME)
	@printf '%s\n' "$(BUILD_DIR)/$(ALPHA_NAME).tar.gz"

ordering-contract-check:
	sh scripts/adapter-ordering-contract-check.sh

e2e: all
	sh e2e/scripts/e2e-check.sh

bili-e2e: all
	sh e2e/scripts/bili-plugin-check.sh

script-contract-e2e: all
	sh e2e/scripts/script-contract-check.sh

bilibili-homepage-demo-e2e: all
	sh e2e/scripts/bilibili-homepage-demo-check.sh

e2e-all: e2e bili-e2e script-contract-e2e bilibili-homepage-demo-e2e

clean:
	rm -rf $(BUILD_DIR)
