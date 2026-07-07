#include "test_harness.h"
#include "mitm_anixops.h"

#include <string.h>

static void add_test(anixops_test_case_t *tests, size_t *count, size_t cap, const char *name, anixops_test_fn fn)
{
	if (*count >= cap) {
		fprintf(stderr, "test registry capacity exceeded\n");
		anixops_test_failures++;
		return;
	}
	tests[*count].name = name;
	tests[*count].fn = fn;
	(*count)++;
}

static void version_is_stable(void)
{
	ANIXOPS_EXPECT_STREQ(anixops_version(), "0.2.0");
	ANIXOPS_EXPECT_EQ_INT(ANIXOPS_VERSION_MAJOR, 0);
	ANIXOPS_EXPECT_EQ_INT(ANIXOPS_VERSION_MINOR, 2);
	ANIXOPS_EXPECT_EQ_INT(ANIXOPS_VERSION_PATCH, 0);
}

static void null_arguments_are_rejected(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t mitm;
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header_rewrite;
	anixops_script_result_t script;
	char body[16];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(NULL, ""), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(NULL, ""), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_script_rule(NULL, ""), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_script_rule(engine, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_argument(NULL, ""), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_argument(engine, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(NULL, "A", "B"), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(engine, NULL, "B"), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(engine, "A", NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(NULL, ""), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(NULL, "example.com", 0, &mitm), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, NULL, 0, &mitm), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "example.com", 0, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(NULL, "https://example.com", ANIXOPS_PHASE_REQUEST, &rewrite), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, NULL, ANIXOPS_PHASE_REQUEST, &rewrite), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(NULL, "https://example.com", ANIXOPS_PHASE_REQUEST, "{}", body, sizeof(body), &rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(engine, NULL, ANIXOPS_PHASE_REQUEST, "{}", body, sizeof(body), &rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, NULL, body, sizeof(body), &rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, "{}", NULL, sizeof(body), &rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, "{}", body, 0, &rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, "{}", body, sizeof(body), NULL),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(NULL, "https://example.com", ANIXOPS_PHASE_REQUEST, 0, "", &header_rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, NULL, ANIXOPS_PHASE_REQUEST, 0, "", &header_rewrite),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://example.com", ANIXOPS_PHASE_REQUEST, 0, "", NULL),
		ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(NULL, "https://example.com", ANIXOPS_PHASE_RESPONSE, &script), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(engine, NULL, ANIXOPS_PHASE_RESPONSE, &script), ANIXOPS_ERR_INVALID_ARGUMENT);
	ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(engine, "https://example.com", ANIXOPS_PHASE_RESPONSE, NULL), ANIXOPS_ERR_INVALID_ARGUMENT);

	anixops_engine_free(engine);
}

static void default_engine_state_is_conservative(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_DISABLED);

	anixops_engine_free(engine);
}

static void clear_resets_and_engine_remains_usable(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^http://a\\.test https://b.test 302"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_argument(engine, "Feature = switch,true"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://a\\.test requires-body=1, script-path=https://script.test/a.js"),
		ANIXOPS_OK);
	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_h2_mitm_enabled(engine, 0);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);

	anixops_engine_clear(engine);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_DISABLED);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "api.example.com"), ANIXOPS_OK);
	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

void anixops_register_abi_tests(anixops_test_case_t *tests, size_t *count, size_t cap)
{
	add_test(tests, count, cap, "abi/version_is_stable", version_is_stable);
	add_test(tests, count, cap, "abi/null_arguments_are_rejected", null_arguments_are_rejected);
	add_test(tests, count, cap, "abi/default_engine_state_is_conservative", default_engine_state_is_conservative);
	add_test(tests, count, cap, "abi/clear_resets_and_engine_remains_usable", clear_resets_and_engine_remains_usable);
}
