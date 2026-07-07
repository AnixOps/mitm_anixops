CC ?= cc
AR ?= ar

BUILD_DIR ?= build
INCLUDE_DIR := include
SRC := src/mitm_anixops.c
OBJ := $(BUILD_DIR)/mitm_anixops.o
STATIC_LIB := $(BUILD_DIR)/libmitm_anixops.a
SHARED_LIB := $(BUILD_DIR)/libmitm_anixops.so
TEST_BIN := $(BUILD_DIR)/test_link
TEST_SRCS := $(sort $(wildcard tests/*.c))
DEMO_BIN := $(BUILD_DIR)/strategy_chain_demo

CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -fPIC
CPPFLAGS ?= -I$(INCLUDE_DIR)
LDFLAGS_SHARED ?= -shared

.PHONY: all clean test demo demo-check ordering-contract-check e2e bili-e2e script-contract-e2e bilibili-homepage-demo-e2e e2e-all

all: $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ): $(SRC) $(INCLUDE_DIR)/mitm_anixops.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $(STATIC_LIB) $(OBJ)

$(SHARED_LIB): $(OBJ)
	$(CC) $(LDFLAGS_SHARED) -o $(SHARED_LIB) $(OBJ)

$(TEST_BIN): $(TEST_SRCS) $(STATIC_LIB) $(INCLUDE_DIR)/mitm_anixops.h tests/test_harness.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror $(TEST_SRCS) $(STATIC_LIB) -o $(TEST_BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

$(DEMO_BIN): examples/strategy_chain_demo.c $(STATIC_LIB) $(INCLUDE_DIR)/mitm_anixops.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror examples/strategy_chain_demo.c $(STATIC_LIB) -o $(DEMO_BIN)

demo: $(DEMO_BIN)
	$(DEMO_BIN)

demo-check: $(DEMO_BIN)
	$(DEMO_BIN) | tee $(BUILD_DIR)/strategy_chain_demo.out
	grep -F 'mitm tcp decision=1 reason=7 pattern=api.example.com' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'mitm quic decision=2 reason=6 pattern=api.example.com' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'rewrite url action=1 status=302 value=https://new.example/path' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'rewrite response-body action=12 body=clean=1&ok=1' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null
	grep -F 'script kind=2 requires_body=1 path=https://scripts.example/demo.js tag=demo.response argument=Mode=demo' $(BUILD_DIR)/strategy_chain_demo.out >/dev/null

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
