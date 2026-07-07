#ifndef ANIXOPS_TEST_HARNESS_H
#define ANIXOPS_TEST_HARNESS_H

#include <stdio.h>
#include <string.h>

typedef void (*anixops_test_fn)(void);

typedef struct anixops_test_case {
	const char *name;
	anixops_test_fn fn;
} anixops_test_case_t;

#define ANIXOPS_EXPECT_TRUE(expr) \
	do { \
		if (!(expr)) { \
			fprintf(stderr, "%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
			anixops_test_failures++; \
			return; \
		} \
	} while (0)

#define ANIXOPS_EXPECT_EQ_INT(actual, expected) \
	do { \
		int anixops_actual__ = (int)(actual); \
		int anixops_expected__ = (int)(expected); \
		if (anixops_actual__ != anixops_expected__) { \
			fprintf(stderr, "%s:%d: expected %s == %s, got %d vs %d\n", \
				__FILE__, __LINE__, #actual, #expected, anixops_actual__, anixops_expected__); \
			anixops_test_failures++; \
			return; \
		} \
	} while (0)

#define ANIXOPS_EXPECT_EQ_SIZE(actual, expected) \
	do { \
		size_t anixops_actual__ = (size_t)(actual); \
		size_t anixops_expected__ = (size_t)(expected); \
		if (anixops_actual__ != anixops_expected__) { \
			fprintf(stderr, "%s:%d: expected %s == %s, got %zu vs %zu\n", \
				__FILE__, __LINE__, #actual, #expected, anixops_actual__, anixops_expected__); \
			anixops_test_failures++; \
			return; \
		} \
	} while (0)

#define ANIXOPS_EXPECT_STREQ(actual, expected) \
	do { \
		const char *anixops_actual__ = (actual); \
		const char *anixops_expected__ = (expected); \
		if (anixops_actual__ == NULL || anixops_expected__ == NULL || strcmp(anixops_actual__, anixops_expected__) != 0) { \
			fprintf(stderr, "%s:%d: expected %s == %s, got \"%s\" vs \"%s\"\n", \
				__FILE__, __LINE__, #actual, #expected, \
				anixops_actual__ == NULL ? "(null)" : anixops_actual__, \
				anixops_expected__ == NULL ? "(null)" : anixops_expected__); \
			anixops_test_failures++; \
			return; \
		} \
	} while (0)

extern int anixops_test_failures;

void anixops_register_abi_tests(anixops_test_case_t *tests, size_t *count, size_t cap);
void anixops_register_config_tests(anixops_test_case_t *tests, size_t *count, size_t cap);
void anixops_register_mitm_tests(anixops_test_case_t *tests, size_t *count, size_t cap);
void anixops_register_rewrite_tests(anixops_test_case_t *tests, size_t *count, size_t cap);
void anixops_register_script_tests(anixops_test_case_t *tests, size_t *count, size_t cap);

#endif
