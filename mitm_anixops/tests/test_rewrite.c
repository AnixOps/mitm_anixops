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

static void no_match_returns_none_and_rule_index_minus_one(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://a\\.test https://b.test 302"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://c.test", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(result.rule_index, -1);
	ANIXOPS_EXPECT_STREQ(result.message, "no rewrite matched");

	anixops_engine_free(engine);
}

static void first_matching_rule_wins(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://same\\.test https://first.test 302"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://same\\.test https://second.test 307"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://same.test", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_EQ_INT(result.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(result.value, "https://first.test");

	anixops_engine_free(engine);
}

static void redirect_302_literal_replacement(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https?:\\/\\/(www.)?(g|google)\\.cn https://www.google.com 302"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "http://google.cn/search?q=x", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 302);
	ANIXOPS_EXPECT_STREQ(result.value, "https://www.google.com");

	anixops_engine_free(engine);
}

static void redirect_307_supports_dollar_capture(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://old\\.test/(.*) https://new.test/$1 307"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://old.test/a/b", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_307);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 307);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/a/b");

	anixops_engine_free(engine);
}

static void redirect_307_supports_backslash_capture(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://old\\.test/(.*) https://new.test/\\1 307"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://old.test/a/b", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_307);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/a/b");

	anixops_engine_free(engine);
}

static void reject_variants_map_to_expected_actions(void)
{
	struct reject_case {
		const char *token;
		anixops_rewrite_action_t action;
		int status_code;
	} cases[] = {
		{"reject", ANIXOPS_REWRITE_REJECT, 0},
		{"reject-200", ANIXOPS_REWRITE_REJECT_200, 200},
		{"reject-img", ANIXOPS_REWRITE_REJECT_IMG, 200},
		{"reject-video", ANIXOPS_REWRITE_REJECT_VIDEO, 200},
		{"reject-dict", ANIXOPS_REWRITE_REJECT_DICT, 200},
		{"reject-array", ANIXOPS_REWRITE_REJECT_ARRAY, 200},
		{"reject-current-session", ANIXOPS_REWRITE_REJECT, 0},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char line[256];
		anixops_engine_t *engine = anixops_engine_new();
		anixops_rewrite_result_t result;
		ANIXOPS_EXPECT_TRUE(engine != NULL);
		snprintf(line, sizeof(line), "^https://case%zu\\.test %s", i, cases[i].token);
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, line), ANIXOPS_OK);
		snprintf(line, sizeof(line), "https://case%zu.test", i);
		ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, line, ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(result.action, cases[i].action);
		ANIXOPS_EXPECT_EQ_INT(result.status_code, cases[i].status_code);
		anixops_engine_free(engine);
	}
}

static void response_phase_rules_do_not_match_request_phase(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://body\\.test mock-response-body {\"ok\":true}"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://body.test", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://body.test", ANIXOPS_PHASE_RESPONSE, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);
	ANIXOPS_EXPECT_STREQ(result.value, "{\"ok\":true}");

	anixops_engine_free(engine);
}

static void mock_request_body_rewrites_body_buffer(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/v1/(.*) {\"mock\":\"$1\"} mock-request-body"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/v1/profile",
			ANIXOPS_PHASE_REQUEST,
			"{\"original\":true}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_REQUEST_BODY);
	ANIXOPS_EXPECT_EQ_INT(result.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(body, "{\"mock\":\"profile\"}");

	anixops_engine_free(engine);
}

static void mock_response_body_rewrites_only_response_phase(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/response mock-response-body {\"ok\":true}"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/response",
			ANIXOPS_PHASE_REQUEST,
			"{\"original\":true}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "{\"original\":true}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/response",
			ANIXOPS_PHASE_RESPONSE,
			"{\"original\":true}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "{\"ok\":true}");

	anixops_engine_free(engine);
}

static void body_apply_reports_truncation_without_overflow(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[6];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/long mock-response-body abcdefgh"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/long",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_STREQ(body, "abcde");
	ANIXOPS_EXPECT_STREQ(result.message, "body truncated");

	anixops_engine_free(engine);
}

static void request_body_replace_regex_applies_all_matches(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/items request-body-replace-regex item=([0-9]+) item=$1x"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/items",
			ANIXOPS_PHASE_REQUEST,
			"item=1&item=23&other=9",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(result.message, "body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "item=1x&item=23x&other=9");

	anixops_engine_free(engine);
}

static void response_body_replace_regex_supports_backslash_capture(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/profile response-body-replace-regex \"name=([A-Za-z]+)\" \"user=\\1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_RESPONSE,
			"name=Alice",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "user=Alice");

	anixops_engine_free(engine);
}

static void body_replace_regex_preserves_body_when_body_pattern_does_not_match(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/nohit request-body-replace-regex missing found"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/nohit",
			ANIXOPS_PHASE_REQUEST,
			"original body",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(result.message, "body regex not matched");
	ANIXOPS_EXPECT_STREQ(body, "original body");

	anixops_engine_free(engine);
}

static void body_replace_regex_respects_phase_and_rejects_invalid_body_regex(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/phase response-body-replace-regex original changed"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/phase",
			ANIXOPS_PHASE_REQUEST,
			"original",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "original");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/bad request-body-replace-regex [ replacement"),
		ANIXOPS_ERR_REGEX);

	anixops_engine_free(engine);
}

static void header_rewrite_rules_are_separate_from_url_rewrite(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t url_result;
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(header) header-replace X-Test value-$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/header", ANIXOPS_PHASE_REQUEST, &url_result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(url_result.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/header", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_REPLACE);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Test");
	ANIXOPS_EXPECT_STREQ(header.value, "value-header");

	anixops_engine_free(engine);
}

static void header_rewrite_enumerates_matching_rules_with_start_index(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) header-add X-One one-$1"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) header-replace X-Two two-$1"), ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/path", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-One");
	ANIXOPS_EXPECT_STREQ(header.value, "one-path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.test/path",
			ANIXOPS_PHASE_REQUEST,
			(size_t)header.rule_index + 1,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_REPLACE);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, 1);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Two");
	ANIXOPS_EXPECT_STREQ(header.value, "two-path");

	anixops_engine_free(engine);
}

static void response_header_rules_respect_phase_and_delete(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/resp response-header-del Set-Cookie"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/resp", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/resp", ANIXOPS_PHASE_RESPONSE, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_DEL);
	ANIXOPS_EXPECT_EQ_INT(header.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_STREQ(header.header_name, "Set-Cookie");
	ANIXOPS_EXPECT_STREQ(header.value, "");

	anixops_engine_free(engine);
}

static void response_header_replace_regex_applies_current_value(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/profile response-header-replace-regex X-Name \"user=([A-Za-z]+)\" \"name=\\1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"user=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Name");
	ANIXOPS_EXPECT_STREQ(header.value, "name=Alice");
	ANIXOPS_EXPECT_STREQ(header.message, "header rewritten");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"no-match",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "no-match");
	ANIXOPS_EXPECT_STREQ(header.message, "header regex not matched");

	anixops_engine_free(engine);
}

static void invalid_header_replace_regex_is_rejected(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/bad response-header-replace-regex X-Bad [ value"),
		ANIXOPS_ERR_REGEX);

	anixops_engine_free(engine);
}

static void long_replacement_stays_within_fixed_buffer(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char long_replacement[2300];
	char rule[2500];
	size_t i;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	for (i = 0; i < sizeof(long_replacement) - 1; i++) {
		long_replacement[i] = 'a';
	}
	long_replacement[sizeof(long_replacement) - 1] = '\0';
	snprintf(rule, sizeof(rule), "^https://long\\.test %s 302", long_replacement);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, rule), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://long.test", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_TRUE(strlen(result.value) < sizeof(result.value));

	anixops_engine_free(engine);
}

void anixops_register_rewrite_tests(anixops_test_case_t *tests, size_t *count, size_t cap)
{
	add_test(tests, count, cap, "rewrite/no_match_returns_none_and_rule_index_minus_one", no_match_returns_none_and_rule_index_minus_one);
	add_test(tests, count, cap, "rewrite/first_matching_rule_wins", first_matching_rule_wins);
	add_test(tests, count, cap, "rewrite/redirect_302_literal_replacement", redirect_302_literal_replacement);
	add_test(tests, count, cap, "rewrite/redirect_307_supports_dollar_capture", redirect_307_supports_dollar_capture);
	add_test(tests, count, cap, "rewrite/redirect_307_supports_backslash_capture", redirect_307_supports_backslash_capture);
	add_test(tests, count, cap, "rewrite/reject_variants_map_to_expected_actions", reject_variants_map_to_expected_actions);
	add_test(tests, count, cap, "rewrite/response_phase_rules_do_not_match_request_phase", response_phase_rules_do_not_match_request_phase);
	add_test(tests, count, cap, "rewrite/mock_request_body_rewrites_body_buffer", mock_request_body_rewrites_body_buffer);
	add_test(tests, count, cap, "rewrite/mock_response_body_rewrites_only_response_phase", mock_response_body_rewrites_only_response_phase);
	add_test(tests, count, cap, "rewrite/body_apply_reports_truncation_without_overflow", body_apply_reports_truncation_without_overflow);
	add_test(tests, count, cap, "rewrite/request_body_replace_regex_applies_all_matches", request_body_replace_regex_applies_all_matches);
	add_test(
		tests,
		count,
		cap,
		"rewrite/response_body_replace_regex_supports_backslash_capture",
		response_body_replace_regex_supports_backslash_capture);
	add_test(
		tests,
		count,
		cap,
		"rewrite/body_replace_regex_preserves_body_when_body_pattern_does_not_match",
		body_replace_regex_preserves_body_when_body_pattern_does_not_match);
	add_test(
		tests,
		count,
		cap,
		"rewrite/body_replace_regex_respects_phase_and_rejects_invalid_body_regex",
		body_replace_regex_respects_phase_and_rejects_invalid_body_regex);
	add_test(
		tests,
		count,
		cap,
		"rewrite/header_rewrite_rules_are_separate_from_url_rewrite",
		header_rewrite_rules_are_separate_from_url_rewrite);
	add_test(
		tests,
		count,
		cap,
		"rewrite/header_rewrite_enumerates_matching_rules_with_start_index",
		header_rewrite_enumerates_matching_rules_with_start_index);
	add_test(
		tests,
		count,
		cap,
		"rewrite/response_header_rules_respect_phase_and_delete",
		response_header_rules_respect_phase_and_delete);
	add_test(
		tests,
		count,
		cap,
		"rewrite/response_header_replace_regex_applies_current_value",
		response_header_replace_regex_applies_current_value);
	add_test(tests, count, cap, "rewrite/invalid_header_replace_regex_is_rejected", invalid_header_replace_regex_is_rejected);
	add_test(tests, count, cap, "rewrite/long_replacement_stays_within_fixed_buffer", long_replacement_stays_within_fixed_buffer);
}
