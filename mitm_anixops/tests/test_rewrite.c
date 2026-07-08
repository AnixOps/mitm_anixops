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

static void set_header_field(anixops_header_field_t *field, const char *name, const char *value)
{
	memset(field, 0, sizeof(*field));
	snprintf(field->name, sizeof(field->name), "%s", name);
	snprintf(field->value, sizeof(field->value), "%s", value);
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
	ANIXOPS_EXPECT_STREQ(result.message, "rewrite matched");

	anixops_engine_free(engine);
}

static void redirect_307_supports_dollar_capture(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://old\\.test/(.*) https://new.test/$1 307"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://brace\\.test/(.*) https://new.test/${1}/done 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_rewrite_evaluate_url(engine, "https://old.test/a/b", ANIXOPS_PHASE_REQUEST, &result), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_307);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 307);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/a/b");
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://brace.test/a/b", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/a/b/done");

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

static void standard_redirect_status_variants_are_supported(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://permanent\\.test/(.*) https://new.test/$1 301"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://see-other\\.test/(.*) url 303 https://new.test/$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://preserve\\.test/(.*) https://new.test/$1 308"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://invalid\\.test/(.*) https://new.test/$1 309"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://permanent.test/a", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_301);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 301);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/a");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://see-other.test/b", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_303);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 303);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/b");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://preserve.test/c", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_308);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 308);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/c");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://invalid.test/d", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);

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
		{"reject-401", ANIXOPS_REWRITE_REJECT, 401},
		{"reject-img", ANIXOPS_REWRITE_REJECT_IMG, 200},
		{"reject-tinygif", ANIXOPS_REWRITE_REJECT_IMG, 200},
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

static void numeric_reject_status_variants_are_supported(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://gone\\.test reject-404"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://legal\\.test reject-451"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://bad\\.test reject-099"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://gone.test", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 404);
	ANIXOPS_EXPECT_STREQ(result.message, "rewrite matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://legal.test", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 451);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://bad.test", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
}

static void quantumultx_url_prefixed_rewrites_are_supported(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://old\\.test/(.*) url 302 https://new.test/$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://ads\\.test url reject-dict"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test url response-body-replace-regex old new"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://json\\.test url response-body-json-replace $.enabled true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 4);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://old.test/path", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(result.value, "https://new.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.test", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REJECT_DICT);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_RESPONSE,
			"old value",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "new value");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://json.test",
			ANIXOPS_PHASE_RESPONSE,
			"{\"enabled\":false,\"name\":\"old\"}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":true,\"name\":\"old\"}");

	anixops_engine_free(engine);
}

static void quantumultx_url_prefixed_header_rewrites_are_supported(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://header\\.test url header-add X-Test value"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test url response-header-replace-regex X-Mode \"mode=([A-Za-z]+)\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://header.test", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Test");
	ANIXOPS_EXPECT_STREQ(header.value, "value");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			1,
			"mode=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "mode=Fast");

	anixops_engine_free(engine);
}

static void quantumultx_echo_response_is_supported(void)
{
	const char *config =
		"[rewrite_local]\n"
		"^https://echo\\.test/(.*) url echo-response text/plain hello-$1\n"
		"^https://direct\\.test/(.*) echo-response application/json direct-$1\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://echo.test/path",
			ANIXOPS_PHASE_REQUEST,
			"original",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "original");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://echo.test/path",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "hello-path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://direct.test/item",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(result.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "direct-item");

	anixops_engine_free(engine);
}

static void unsupported_recognized_rewrite_actions_are_ignored(void)
{
	const char *config =
		"[Rewrite]\n"
		"^https://map\\.test map-local /tmp/body.json\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	int status = ANIXOPS_ERR_PARSE;
	size_t line = 123;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(line, 0);
	ANIXOPS_EXPECT_STREQ(message, "ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://map.test", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
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

static void regex_replacement_empty_matches_are_stable(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	anixops_header_rewrite_result_t header;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test/start request-body-replace-regex ^ start-"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test/end request-body-replace-regex $ -end"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test/lazy request-body-replace-regex \".*?\" all"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://header\\.test/start response-header-replace-regex X-Test ^ start-"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://header\\.test/end response-header-replace-regex X-Test $ -end"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test/start",
			ANIXOPS_PHASE_REQUEST,
			"abc",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "start-abc");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test/end",
			ANIXOPS_PHASE_REQUEST,
			"abc",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "abc-end");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test/lazy",
			ANIXOPS_PHASE_REQUEST,
			"abc",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "all");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test/start",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"value",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "start-value");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test/end",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"value",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "value-end");

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

static void inline_case_insensitive_regex_prefix_matches_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "(?i)^https://api\\.test/(item) https://dest.test/$1 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex (?i)token ok"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"(?i)user=([a-z]+)\" \"name=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response (?i)^https://script\\.test script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://API.test/ITEM", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/ITEM");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"TOKEN=1",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "ok=1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"USER=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "name=Alice");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://SCRIPT.test", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void inline_dotall_regex_prefix_matches_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "(?s)^https://dotall\\.test/a.*b$ https://dest.test/dotall 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"(?s)line.*next\" ok"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"(?is)mode.*=([a-z]+)$\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(
			engine,
			"http-response (?s)^https://script\\.test/a.*b$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://dotall.test/a\nb", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/dotall");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"line\nmiddle\nnext!",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "ok!");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"MODE\n=FAST",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=FAST");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/a\nb", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void inline_multiline_regex_prefix_matches_line_anchors(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"(?m)^token=([0-9]+)$\" \"id=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"(?im)^mode=([a-z]+)$\" \"mode=$1\""),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"start\ntoken=42\nend",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "start\nid=42\nend");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"start\nMODE=Fast\nend",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "start\nmode=Fast\nend");

	anixops_engine_free(engine);
}

static void pcre_shorthand_regex_classes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/item/(\\d+)$ https://dest.test/$1 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"id=\\d+\\s+name=\\w+\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"token=(\\w+)\" \"token=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/\\w+$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item/12345", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/12345");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"id=42 name=Alice",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"token=abc_123",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "token=abc_123");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/name_123", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_negated_shorthand_regex_classes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/item/(\\D)(\\W)(\\S)$ https://dest.test/$1$2$3 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"code=\\D\\W\\S\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"token=(\\D)(\\W)(\\S)\" \"token=$1$2$3\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/\\D\\W\\S$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item/a-!", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/a-!");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"code=a-!",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"token=a-!",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "token=a-!");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/a-!", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_horizontal_whitespace_classes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://blank\\.test/a\\hb\\H$ https://dest.test/blank 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"key\\h*=\\h*\\H+\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode\\h*=\\h*(\\H+)$\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/a\\hb\\H$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://blank.test/a bX", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/blank");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"key\t= value!",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode = Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/a\tbZ", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_vertical_whitespace_classes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://vert\\.test/a\\vb\\V$ https://dest.test/vert 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"top\\v+bottom\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode\\v*=\\V+\" mode=ok"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/a\\vb\\V$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://vert.test/a\nbX", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/vert");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"top\r\nbottom",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode\n=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/a\rbZ", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_hex_byte_escapes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\x2etest/(item)$ https://dest.test/$1 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"id\\x3d[0-9]+\\x20name\\x3d[A-Za-z]+\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode\\x3d([A-Za-z]+)\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\x2etest/path script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://bad\\.test request-body-replace-regex \\x00 value"),
		ANIXOPS_ERR_REGEX);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/item");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"id=42 name=Alice",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/path", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_unicode_escapes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\u002etest/caf\\u00e9$ https://dest.test/ok 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"name\\u003dcaf\\u00e9\" matched"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode\\u003d(.+)$\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(
			engine,
			"http-response ^https://script\\u002etest/caf\\u00e9$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://bad\\.test request-body-replace-regex \\u0000 value"),
		ANIXOPS_ERR_REGEX);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/caf" "\xC3" "\xA9", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"name=caf" "\xC3" "\xA9" "; ok",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "matched; ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode=caf" "\xC3" "\xA9",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=caf" "\xC3" "\xA9");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/caf" "\xC3" "\xA9", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_control_escapes_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://tab\\.test/a\\tb$ https://dest.test/tab 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"line\\nnext\" ok"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode\\r\\n(.+)$\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/a\\tb$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://tab.test/a\tb", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/tab");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"line\nnext!",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "ok!");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode\r\nFast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/a\tb", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_lazy_quantifiers_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*?)$ https://dest.test/$1 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"id=[0-9]{1,3}?;\" \"id=ok;\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"mode=(.+?)$\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://script\\.test/.+?$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://literal\\.test/\\*?done$ https://literal.test/ok 302"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/path/file", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/path/file");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"id=42; done",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "id=ok; done");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"mode=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/item", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://literal.test/done", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://literal.test/ok");

	anixops_engine_free(engine);
}

static void pcre_non_capturing_groups_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://(?:api|m)\\.test/(item)/(v[0-9]+)$ https://dest.test/$1/$2 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"(?:token|id)=([0-9]+)\" value=$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"(?:token|id)=([A-Za-z]+)\" \"name=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://(?:script|cdn)\\.test/path script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item/v1", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/item/v1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"id=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "value=42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"token=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "name=Alice");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://cdn.test/path", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre_absolute_anchors_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "\\Ahttps://api\\.test/item/([0-9]+)\\z https://dest.test/$1 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"\\Atoken=([0-9]+)\\z\" value=$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"\\Aname=([A-Za-z]+)\\Z\" \"user=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response \\Ahttps://script\\.test/path\\z script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item/42", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "xhttps://api.test/item/42", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "value=42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"x token=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(rewrite.message, "body regex not matched");
	ANIXOPS_EXPECT_STREQ(body, "x token=42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"name=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "user=Alice");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/path", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/path/more", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);

	anixops_engine_free(engine);
}

static void pcre_named_capture_groups_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://(?<sub>api)\\.test/(item)$ https://dest.test/$1/$2 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"(?<key>token)=([0-9]+)\" value=$1:$2"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"(?'name'user)=([A-Za-z]+)\" \"$1=$2\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://(?<name>script)\\.test/path script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/api/item");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "value=token:42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"user=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "user=Alice");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/path", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void named_capture_replacements_are_supported_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://(?<host>api)\\.named\\.test/(?<item>[A-Za-z0-9]+)$ https://dest.test/${host}/$<item> 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.named\\.test request-body-replace-regex \"(?<key>token)=(?<value>[0-9]+)\" "
			"\"${key}:$<value>\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.named\\.test response-header-replace-regex X-Test "
			"\"(?'prefix'user)=(?<name>[A-Za-z]+)\" \"$<prefix>=${name}\""),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://api.named.test/item42",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/api/item42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.named.test",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "token:42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.named.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"user=Alice",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "user=Alice");

	anixops_engine_free(engine);
}

static void pcre_quoted_literals_match_all_regex_contexts(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/\\Qitem.v1+test?\\E$ https://dest.test/ok 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://body\\.test request-body-replace-regex \"\\Qtoken[0]\\E=([0-9]+)\" value=$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://header\\.test response-header-replace-regex X-Test \"\\Qx.mode\\E=([A-Za-z]+)\" \"mode=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response ^https://\\Qscript.test/path(1)\\E$ script-path=https://x.test/a.js"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item.v1+test?", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"token[0]=42",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "value=42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://header.test",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"x.mode=fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.value, "mode=fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.test/path(1)", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://x.test/a.js");

	anixops_engine_free(engine);
}

static void pcre2_backend_matches_lookaround_when_available(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	if (!anixops_regex_backend_available(ANIXOPS_REGEX_BACKEND_PCRE2)) {
		anixops_engine_free(engine);
		return;
	}

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_regex_backend(engine, ANIXOPS_REGEX_BACKEND_PCRE2), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/(?=item)(item)/(\\d+)$ https://dest.test/$1/$2 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://body\\.test request-body-replace-regex \"(?<=token=)(\\d+)\" \"$1-ok\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://word\\.test request-body-replace-regex \"\\btoken\\b\" word"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://api.test/item/42", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://dest.test/item/42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://body.test",
			ANIXOPS_PHASE_REQUEST,
			"token=123&ok=1",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "token=123-ok&ok=1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://word.test",
			ANIXOPS_PHASE_REQUEST,
			"token tokenized",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "word tokenized");

	anixops_engine_free(engine);
}

static void request_body_json_replace_updates_top_level_field(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/profile request-body-json-replace $.enabled true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_REQUEST,
			"{\"enabled\":false,\"name\":\"old\"}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":true,\"name\":\"old\"}");

	anixops_engine_free(engine);
}

static void body_rewrite_chain_applies_matching_rules_in_order(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_body_rewrite_chain_t chain;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/chain https://other.test 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/chain request-body-replace-regex \"from=([0-9]+)\" \"mid=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/chain request-body-replace-regex \"mid=([0-9]+)\" \"to=$1\""),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body_chain(
			engine,
			"https://api.test/chain",
			ANIXOPS_PHASE_REQUEST,
			"from=42",
			body,
			sizeof(body),
			&chain),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_STREQ(body, "to=42");
	ANIXOPS_EXPECT_EQ_SIZE(chain.rewrite_count, 2);
	ANIXOPS_EXPECT_EQ_INT(chain.rewritten, 1);
	ANIXOPS_EXPECT_EQ_INT(chain.truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(chain.rewrites[0].action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(chain.rewrites[0].rule_index, 1);
	ANIXOPS_EXPECT_STREQ(chain.rewrites[0].message, "body rewritten");
	ANIXOPS_EXPECT_EQ_INT(chain.rewrites[1].action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(chain.rewrites[1].rule_index, 2);
	ANIXOPS_EXPECT_STREQ(chain.rewrites[1].message, "body rewritten");

	anixops_engine_free(engine);
}

static void legacy_apply_body_keeps_first_body_rule_behavior(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	anixops_body_rewrite_chain_t chain;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/legacy request-body-replace-regex \"from=([0-9]+)\" \"mid=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/legacy request-body-replace-regex \"mid=([0-9]+)\" \"to=$1\""),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/legacy",
			ANIXOPS_PHASE_REQUEST,
			"from=42",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(result.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(body, "mid=42");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body_chain(
			engine,
			"https://api.test/legacy",
			ANIXOPS_PHASE_REQUEST,
			"from=42",
			body,
			sizeof(body),
			&chain),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_STREQ(body, "to=42");
	ANIXOPS_EXPECT_EQ_SIZE(chain.rewrite_count, 2);

	anixops_engine_free(engine);
}

static void jq_body_rewrite_rules_are_matched_with_fail_open_runtime_gap(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/request request-body-jq '.enabled = true'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/response http-response-jq '.enabled = false'"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/request",
			ANIXOPS_PHASE_REQUEST,
			"{\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JQ);
#if defined(ANIXOPS_ENABLE_LIBJQ)
	ANIXOPS_EXPECT_STREQ(result.message, "jq body rewritten");
	ANIXOPS_EXPECT_STREQ(result.value, ".enabled = true");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":true}");
#else
	ANIXOPS_EXPECT_STREQ(result.message, "jq backend unavailable");
	ANIXOPS_EXPECT_STREQ(result.value, ".enabled = true");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":false}");
#endif

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/response",
			ANIXOPS_PHASE_RESPONSE,
			"{\"enabled\":true}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JQ);
#if defined(ANIXOPS_ENABLE_LIBJQ)
	ANIXOPS_EXPECT_STREQ(result.message, "jq body rewritten");
	ANIXOPS_EXPECT_STREQ(result.value, ".enabled = false");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":false}");
#else
	ANIXOPS_EXPECT_STREQ(result.message, "jq backend unavailable");
	ANIXOPS_EXPECT_STREQ(result.value, ".enabled = false");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":true}");
#endif

	anixops_engine_free(engine);
}

#if defined(ANIXOPS_ENABLE_LIBJQ)
static void jq_backend_handles_output_and_error_policy(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/jq/multi response-body-jq '.items[]'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/jq/empty response-body-jq 'empty'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/jq/bad-json response-body-jq '.'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/jq/bad-filter response-body-jq '['"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/jq/multi",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[1,2]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JQ);
	ANIXOPS_EXPECT_STREQ(result.message, "jq body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/jq/empty",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[1]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JQ);
	ANIXOPS_EXPECT_STREQ(result.message, "jq produced no output");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[1]}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/jq/bad-json",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JQ);
	ANIXOPS_EXPECT_STREQ(result.message, "jq execution failed");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/jq/bad-filter",
			ANIXOPS_PHASE_RESPONSE,
			"{\"ok\":true}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JQ);
	ANIXOPS_EXPECT_STREQ(result.message, "jq execution failed");
	ANIXOPS_EXPECT_STREQ(body, "{\"ok\":true}");

	anixops_engine_free(engine);
}
#endif

static void response_body_json_replace_supports_nested_path_and_missing_path(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/profile response-body-json-replace $.user.name '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/missing response-body-json-replace $.missing true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/array response-body-json-replace $.items[1].title '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/out response-body-json-replace $.items[5].title true"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_RESPONSE,
			"{\"user\":{\"name\":\"Alice\",\"id\":7},\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"user\":{\"name\":\"test\",\"id\":7},\"enabled\":false}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/missing",
			ANIXOPS_PHASE_RESPONSE,
			"{\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path not matched");
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":false}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/array",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[{\"title\":\"a\"},{\"title\":\"b\"}],\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[{\"title\":\"a\"},{\"title\":\"test\"}],\"enabled\":false}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/out",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[{\"title\":\"a\"}]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path not matched");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[{\"title\":\"a\"}]}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_REQUEST,
			"{\"user\":{\"name\":\"Alice\"}}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "{\"user\":{\"name\":\"Alice\"}}");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_array_index_and_reports_truncation(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[20];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/items request-body-json-replace $.items[0] true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/bad-path request-body-json-replace $.items[] true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/small request-body-json-replace $.name '\"long-value\"'"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/items",
			ANIXOPS_PHASE_REQUEST,
			"{\"items\":[false]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[true]}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/bad-path",
			ANIXOPS_PHASE_REQUEST,
			"{\"items\":[false]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path unsupported");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[false]}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/small",
			ANIXOPS_PHASE_REQUEST,
			"{\"name\":\"old\",\"x\":1}",
			body,
			10,
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "body truncated");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_negative_array_index(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/items response-body-json-replace $.items[-1].title '\"tail\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/out response-body-json-replace $.items[-3].title '\"none\"'"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/items",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[{\"title\":\"a\"},{\"title\":\"b\"}],\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[{\"title\":\"a\"},{\"title\":\"tail\"}],\"enabled\":false}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/out",
			ANIXOPS_PHASE_RESPONSE,
			"{\"items\":[{\"title\":\"a\"},{\"title\":\"b\"}]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path not matched");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[{\"title\":\"a\"},{\"title\":\"b\"}]}");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_bracket_string_keys(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/profile request-body-json-replace $['profile.meta']['name'] '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/items request-body-json-replace $['items'][1]['title'] '\"test\"'"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/profile",
			ANIXOPS_PHASE_REQUEST,
			"{\"profile.meta\":{\"name\":\"Alice\",\"id\":7}}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"profile.meta\":{\"name\":\"test\",\"id\":7}}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/items",
			ANIXOPS_PHASE_REQUEST,
			"{\"items\":[{\"title\":\"a\"},{\"title\":\"b\"}]}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"items\":[{\"title\":\"a\"},{\"title\":\"test\"}]}");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_empty_bracket_string_keys(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/empty request-body-json-replace $['']['name'] '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/missing request-body-json-replace $['']['missing'] true"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/empty",
			ANIXOPS_PHASE_REQUEST,
			"{\"\":{\"name\":\"Alice\",\"id\":7},\"enabled\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"\":{\"name\":\"test\",\"id\":7},\"enabled\":false}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/missing",
			ANIXOPS_PHASE_REQUEST,
			"{\"\":{\"name\":\"Alice\"}}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path not matched");
	ANIXOPS_EXPECT_STREQ(body, "{\"\":{\"name\":\"Alice\"}}");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_escaped_bracket_string_keys(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/quote request-body-json-replace $['profile\\'meta']['name'] '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/slash request-body-json-replace $[\"path\\\\key\"] true"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/quote",
			ANIXOPS_PHASE_REQUEST,
			"{\"profile'meta\":{\"name\":\"Alice\"}}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"profile'meta\":{\"name\":\"test\"}}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/slash",
			ANIXOPS_PHASE_REQUEST,
			"{\"path\\\\key\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"path\\\\key\":true}");

	anixops_engine_free(engine);
}

static void json_body_replace_supports_unicode_escaped_bracket_string_keys(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[200];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/dot request-body-json-replace $['profile\\u002emeta']['na\\u006de'] '\"test\"'"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/body request-body-json-replace $['caf\\u00e9'] true"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/bad request-body-json-replace $['bad\\u00zz'] true"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/dot",
			ANIXOPS_PHASE_REQUEST,
			"{\"profile.meta\":{\"na\\u006de\":\"Alice\"}}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"profile.meta\":{\"na\\u006de\":\"test\"}}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/body",
			ANIXOPS_PHASE_REQUEST,
			"{\"caf\\u00e9\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json body rewritten");
	ANIXOPS_EXPECT_STREQ(body, "{\"caf\\u00e9\":true}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/bad",
			ANIXOPS_PHASE_REQUEST,
			"{\"bad\":false}",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(result.message, "json path unsupported");
	ANIXOPS_EXPECT_STREQ(body, "{\"bad\":false}");

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

static void named_header_rewrite_matches_header_case_insensitively(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) response-header-replace X-Mode mode-$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/(.*) response-header-replace-regex X-Mode \"old=([A-Za-z]+)\" \"new=$1\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) response-header-replace X-Other other-$1"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_named_header(
			engine,
			"https://api.test/path",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"x-mode",
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "mode-path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_named_header(
			engine,
			"https://api.test/path",
			ANIXOPS_PHASE_RESPONSE,
			(size_t)header.rule_index + 1,
			"X-MODE",
			"old=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, 1);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "new=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_named_header(
			engine,
			"https://api.test/path",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"x-missing",
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, -1);

	anixops_engine_free(engine);
}

static void header_list_rewrite_applies_multi_value_request_semantics(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_list_t input;
	anixops_header_list_t output;
	anixops_rewrite_plan_t plan;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	memset(&input, 0, sizeof(input));
	set_header_field(&input.fields[0], "X-Mode", "old-one");
	set_header_field(&input.fields[1], "x-mode", "old-two");
	set_header_field(&input.fields[2], "X-Drop", "remove-me");
	set_header_field(&input.fields[3], "X-Token", "old=Fast");
	set_header_field(&input.fields[4], "x-token", "skip");
	input.count = 5;

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) header-add X-Trace trace-$1"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) header-replace X-Mode mode-$1"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/(.*) header-del X-Drop"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/(.*) header-replace-regex X-Token \"old=([A-Za-z]+)\" \"new=$1\""),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_headers(engine, "https://api.test/path", ANIXOPS_PHASE_REQUEST, &input, &output, &plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(output.count, 4);
	ANIXOPS_EXPECT_EQ_INT(output.truncated, 0);
	ANIXOPS_EXPECT_STREQ(output.fields[0].name, "X-Token");
	ANIXOPS_EXPECT_STREQ(output.fields[0].value, "new=Fast");
	ANIXOPS_EXPECT_STREQ(output.fields[1].name, "x-token");
	ANIXOPS_EXPECT_STREQ(output.fields[1].value, "skip");
	ANIXOPS_EXPECT_STREQ(output.fields[2].name, "X-Trace");
	ANIXOPS_EXPECT_STREQ(output.fields[2].value, "trace-path");
	ANIXOPS_EXPECT_STREQ(output.fields[3].name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(output.fields[3].value, "mode-path");

	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 4);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, "X-Trace");
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].action, ANIXOPS_REWRITE_HEADER_REPLACE);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].header_name, "X-Mode");
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[2].action, ANIXOPS_REWRITE_HEADER_DEL);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[2].message, "header list deleted");
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[3].action, ANIXOPS_REWRITE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[3].value, "new=Fast");

	anixops_engine_free(engine);
}

static void header_list_rewrite_keeps_set_cookie_values_separate(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_list_t input;
	anixops_header_list_t output;
	anixops_rewrite_plan_t plan;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	memset(&input, 0, sizeof(input));
	set_header_field(&input.fields[0], "Set-Cookie", "a=1; Path=/");
	set_header_field(&input.fields[1], "set-cookie", "b=1; Path=/");
	set_header_field(&input.fields[2], "X-Remove", "yes");
	input.count = 3;

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/cookies response-header-add Set-Cookie \"c=1; Path=/\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(
			engine,
			"^https://api\\.test/cookies response-header-replace-regex Set-Cookie \"a=1\" \"a=2\""),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/cookies response-header-del X-Remove"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_headers(engine, "https://api.test/cookies", ANIXOPS_PHASE_RESPONSE, &input, &output, &plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(output.count, 3);
	ANIXOPS_EXPECT_EQ_INT(output.truncated, 0);
	ANIXOPS_EXPECT_STREQ(output.fields[0].name, "Set-Cookie");
	ANIXOPS_EXPECT_STREQ(output.fields[0].value, "a=2; Path=/");
	ANIXOPS_EXPECT_STREQ(output.fields[1].name, "set-cookie");
	ANIXOPS_EXPECT_STREQ(output.fields[1].value, "b=1; Path=/");
	ANIXOPS_EXPECT_STREQ(output.fields[2].name, "Set-Cookie");
	ANIXOPS_EXPECT_STREQ(output.fields[2].value, "c=1; Path=/");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 3);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, ANIXOPS_REWRITE_RESPONSE_HEADER_ADD);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[2].action, ANIXOPS_REWRITE_RESPONSE_HEADER_DEL);

	anixops_engine_free(engine);
}

static void rewrite_plan_matches_individual_evaluation_order(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_plan_t plan;
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char plan_body[128];
	char individual_body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://plan\\.test/(.*) header-add X-One one-$1"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://plan\\.test/(.*) header-replace X-Two two-$1"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://plan\\.test/(.*) request-body-replace-regex token=([0-9]+) token=$1-ok"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(
			engine,
			"http-request ^https://plan\\.test/(.*) requires-body=1, script-path=https://scripts.test/plan.js, "
			"tag=plan.request, argument=Mode=plan"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://plan.test/path",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			plan_body,
			sizeof(plan_body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, 2);
	ANIXOPS_EXPECT_STREQ(plan_body, "token=42-ok");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 2);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, 0);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, "X-One");
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, "one-path");
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].action, ANIXOPS_REWRITE_HEADER_REPLACE);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].rule_index, 1);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].header_name, "X-Two");
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].value, "two-path");
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(plan.script.script_path, "https://scripts.test/plan.js");
	ANIXOPS_EXPECT_STREQ(plan.script.tag, "plan.request");
	ANIXOPS_EXPECT_STREQ(plan.script.argument, "Mode=plan");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://plan.test/path",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			individual_body,
			sizeof(individual_body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, rewrite.rule_index);
	ANIXOPS_EXPECT_STREQ(plan_body, individual_body);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://plan.test/path", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, header.action);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, header.value);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://plan.test/path",
			ANIXOPS_PHASE_REQUEST,
			(size_t)header.rule_index + 1,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].action, header.action);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].value, header.value);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://plan.test/path", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, script.kind);
	ANIXOPS_EXPECT_STREQ(plan.script.script_path, script.script_path);
	ANIXOPS_EXPECT_STREQ(plan.script.argument, script.argument);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(engine, "https://plan.test/path", ANIXOPS_PHASE_REQUEST, NULL, NULL, 0, NULL, &plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 2);

	anixops_engine_free(engine);
}

static void request_header_delete_is_supported(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://api\\.test/delete header-del X-Trace"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/delete", ANIXOPS_PHASE_RESPONSE, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/delete", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_DEL);
	ANIXOPS_EXPECT_EQ_INT(header.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Trace");
	ANIXOPS_EXPECT_STREQ(header.value, "");
	ANIXOPS_EXPECT_STREQ(header.message, "header rewrite matched");

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
	add_test(
		tests,
		count,
		cap,
		"rewrite/standard_redirect_status_variants_are_supported",
		standard_redirect_status_variants_are_supported);
	add_test(tests, count, cap, "rewrite/reject_variants_map_to_expected_actions", reject_variants_map_to_expected_actions);
	add_test(tests, count, cap, "rewrite/numeric_reject_status_variants_are_supported", numeric_reject_status_variants_are_supported);
	add_test(
		tests,
		count,
		cap,
		"rewrite/quantumultx_url_prefixed_rewrites_are_supported",
		quantumultx_url_prefixed_rewrites_are_supported);
	add_test(
		tests,
		count,
		cap,
		"rewrite/quantumultx_url_prefixed_header_rewrites_are_supported",
		quantumultx_url_prefixed_header_rewrites_are_supported);
	add_test(
		tests,
		count,
		cap,
		"rewrite/quantumultx_echo_response_is_supported",
		quantumultx_echo_response_is_supported);
	add_test(
		tests,
		count,
		cap,
		"rewrite/unsupported_recognized_rewrite_actions_are_ignored",
		unsupported_recognized_rewrite_actions_are_ignored);
	add_test(tests, count, cap, "rewrite/response_phase_rules_do_not_match_request_phase", response_phase_rules_do_not_match_request_phase);
	add_test(tests, count, cap, "rewrite/mock_request_body_rewrites_body_buffer", mock_request_body_rewrites_body_buffer);
	add_test(tests, count, cap, "rewrite/mock_response_body_rewrites_only_response_phase", mock_response_body_rewrites_only_response_phase);
	add_test(tests, count, cap, "rewrite/body_apply_reports_truncation_without_overflow", body_apply_reports_truncation_without_overflow);
	add_test(tests, count, cap, "rewrite/request_body_replace_regex_applies_all_matches", request_body_replace_regex_applies_all_matches);
	add_test(
		tests,
		count,
		cap,
		"rewrite/regex_replacement_empty_matches_are_stable",
		regex_replacement_empty_matches_are_stable);
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
		"rewrite/inline_case_insensitive_regex_prefix_matches_all_regex_contexts",
		inline_case_insensitive_regex_prefix_matches_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/inline_dotall_regex_prefix_matches_all_regex_contexts",
		inline_dotall_regex_prefix_matches_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/inline_multiline_regex_prefix_matches_line_anchors",
		inline_multiline_regex_prefix_matches_line_anchors);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_shorthand_regex_classes_match_all_regex_contexts",
		pcre_shorthand_regex_classes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_negated_shorthand_regex_classes_match_all_regex_contexts",
		pcre_negated_shorthand_regex_classes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_horizontal_whitespace_classes_match_all_regex_contexts",
		pcre_horizontal_whitespace_classes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_vertical_whitespace_classes_match_all_regex_contexts",
		pcre_vertical_whitespace_classes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_hex_byte_escapes_match_all_regex_contexts",
		pcre_hex_byte_escapes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_unicode_escapes_match_all_regex_contexts",
		pcre_unicode_escapes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_control_escapes_match_all_regex_contexts",
		pcre_control_escapes_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_lazy_quantifiers_match_all_regex_contexts",
		pcre_lazy_quantifiers_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_non_capturing_groups_match_all_regex_contexts",
		pcre_non_capturing_groups_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_absolute_anchors_match_all_regex_contexts",
		pcre_absolute_anchors_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_named_capture_groups_match_all_regex_contexts",
		pcre_named_capture_groups_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/named_capture_replacements_are_supported_all_regex_contexts",
		named_capture_replacements_are_supported_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre_quoted_literals_match_all_regex_contexts",
		pcre_quoted_literals_match_all_regex_contexts);
	add_test(
		tests,
		count,
		cap,
		"rewrite/pcre2_backend_matches_lookaround_when_available",
		pcre2_backend_matches_lookaround_when_available);
	add_test(
		tests,
		count,
		cap,
		"rewrite/request_body_json_replace_updates_top_level_field",
		request_body_json_replace_updates_top_level_field);
	add_test(
		tests,
		count,
		cap,
		"rewrite/body_rewrite_chain_applies_matching_rules_in_order",
		body_rewrite_chain_applies_matching_rules_in_order);
	add_test(
		tests,
		count,
		cap,
		"rewrite/legacy_apply_body_keeps_first_body_rule_behavior",
		legacy_apply_body_keeps_first_body_rule_behavior);
	add_test(
		tests,
		count,
		cap,
		"rewrite/jq_body_rewrite_rules_are_matched_with_fail_open_runtime_gap",
		jq_body_rewrite_rules_are_matched_with_fail_open_runtime_gap);
#if defined(ANIXOPS_ENABLE_LIBJQ)
	add_test(
		tests,
		count,
		cap,
		"rewrite/jq_backend_handles_output_and_error_policy",
		jq_backend_handles_output_and_error_policy);
#endif
	add_test(
		tests,
		count,
		cap,
		"rewrite/response_body_json_replace_supports_nested_path_and_missing_path",
		response_body_json_replace_supports_nested_path_and_missing_path);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_array_index_and_reports_truncation",
		json_body_replace_supports_array_index_and_reports_truncation);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_negative_array_index",
		json_body_replace_supports_negative_array_index);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_bracket_string_keys",
		json_body_replace_supports_bracket_string_keys);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_empty_bracket_string_keys",
		json_body_replace_supports_empty_bracket_string_keys);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_escaped_bracket_string_keys",
		json_body_replace_supports_escaped_bracket_string_keys);
	add_test(
		tests,
		count,
		cap,
		"rewrite/json_body_replace_supports_unicode_escaped_bracket_string_keys",
		json_body_replace_supports_unicode_escaped_bracket_string_keys);
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
		"rewrite/named_header_rewrite_matches_header_case_insensitively",
		named_header_rewrite_matches_header_case_insensitively);
	add_test(
		tests,
		count,
		cap,
		"rewrite/header_list_rewrite_applies_multi_value_request_semantics",
		header_list_rewrite_applies_multi_value_request_semantics);
	add_test(
		tests,
		count,
		cap,
		"rewrite/header_list_rewrite_keeps_set_cookie_values_separate",
		header_list_rewrite_keeps_set_cookie_values_separate);
	add_test(tests, count, cap, "rewrite/rewrite_plan_matches_individual_evaluation_order", rewrite_plan_matches_individual_evaluation_order);
	add_test(tests, count, cap, "rewrite/request_header_delete_is_supported", request_header_delete_is_supported);
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
