#include "test_harness.h"

#include <stdio.h>

int anixops_test_failures = 0;

static void add_group(anixops_test_case_t *tests, size_t *count, size_t cap, void (*register_fn)(anixops_test_case_t *, size_t *, size_t))
{
	register_fn(tests, count, cap);
}

int main(void)
{
	anixops_test_case_t tests[384];
	size_t count = 0;
	size_t i;
	size_t failed = 0;

	add_group(tests, &count, sizeof(tests) / sizeof(tests[0]), anixops_register_abi_tests);
	add_group(tests, &count, sizeof(tests) / sizeof(tests[0]), anixops_register_config_tests);
	add_group(tests, &count, sizeof(tests) / sizeof(tests[0]), anixops_register_mitm_tests);
	add_group(tests, &count, sizeof(tests) / sizeof(tests[0]), anixops_register_rewrite_tests);
	add_group(tests, &count, sizeof(tests) / sizeof(tests[0]), anixops_register_script_tests);
	if (anixops_test_failures != 0) {
		return 1;
	}

	for (i = 0; i < count; i++) {
		int before = anixops_test_failures;
		tests[i].fn();
		if (anixops_test_failures == before) {
			printf("ok %zu - %s\n", i + 1, tests[i].name);
		}
		else {
			printf("not ok %zu - %s\n", i + 1, tests[i].name);
			failed++;
		}
	}

	printf("%zu tests, %zu failed\n", count, failed);
	return failed == 0 ? 0 : 1;
}
