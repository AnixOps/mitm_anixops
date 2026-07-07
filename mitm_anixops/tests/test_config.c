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

static void config_parses_rewrite_and_mitm_sections(void)
{
	const char *config =
		"# comment\n"
		"[Rewrite]\n"
		"^http://old\\.test/(.*) https://new.test/$1 302\n"
		"\n"
		"[MITM]\n"
		"hostname = *.example.com, -secure.example.com\n";
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	anixops_engine_free(engine);
}

static void config_accepts_section_aliases_and_crlf(void)
{
	const char *config =
		"[URL Rewrite]\r\n"
		"^http://a\\.test https://b.test 302\r\n"
		"[Remote Rewrite]\r\n"
		"^http://c\\.test https://d.test 307\r\n"
		"[Mitm]\r\n"
		"hostname = api.test\r\n";
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);

	anixops_engine_free(engine);
}

static void config_accepts_header_rewrite_section_aliases(void)
{
	const char *config =
		"[Header Rewrite]\n"
		"^https://api\\.test/request header-add X-Test value\n"
		"[Remote Header Rewrite]\n"
		"^https://api\\.test/response response-header-del Set-Cookie\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/request", ANIXOPS_PHASE_REQUEST, 0, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Test");
	ANIXOPS_EXPECT_STREQ(header.value, "value");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(engine, "https://api.test/response", ANIXOPS_PHASE_RESPONSE, 1, NULL, &header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_DEL);
	ANIXOPS_EXPECT_STREQ(header.header_name, "Set-Cookie");

	anixops_engine_free(engine);
}

static void config_accepts_quantumultx_rewrite_remote_section_aliases(void)
{
	const char *config =
		"#[rewrite_remote]\n"
		"^https://remote\\.qx\\.test/block reject\n"
		"[rewrite_remote]\n"
		"^https://remote\\.qx\\.test/old https://remote.qx.test/new 302\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://remote.qx.test/block", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REJECT);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://remote.qx.test/old", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(result.value, "https://remote.qx.test/new");

	anixops_engine_free(engine);
}

static void config_accepts_h2_enable_mitm_key_alias(void)
{
	const char *config =
		"[MITM]\n"
		"hostname = api.example.test\n"
		"h2_enable = false\n";
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 0);

	anixops_engine_free(engine);
}

static void config_accepts_disable_quic_mitm_key_aliases(void)
{
	const char *config =
		"[MITM]\n"
		"enable = true\n"
		"hostname = api.example.test\n"
		"disable_quic = false\n"
		"disable_mitm_quic = false\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.test", 1, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

static void config_parses_script_and_ignores_unknown_sections(void)
{
	const char *config =
		"[Script]\n"
		"http-request ^https://script\\.test script-path=https://script.test/a.js\n"
		"[General]\n"
		"hostname = ignored.test\n"
		"[Rewrite]\n"
		"// comment style\n"
		"^https://ads\\.test reject\n";
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	anixops_engine_free(engine);
}

static void incomplete_rewrite_lines_are_ignored(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, ""), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://example\\.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://example\\.com https://target.test"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);

	anixops_engine_free(engine);
}

static void invalid_regex_is_reported_and_does_not_add_rule(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "[ https://target.test 302"), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_rewrite_rule(engine, "^https://ok\\.test https://target.test 302"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);

	anixops_engine_free(engine);
}

static void config_failure_records_line_diagnostic(void)
{
	const char *config =
		"[Rewrite]\n"
		"\n"
		"# blank line above must still count\n"
		"[ https://target.test 302\n";
	anixops_engine_t *engine = anixops_engine_new();
	int status = 0;
	size_t line = 0;
	char message[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 4);
	ANIXOPS_EXPECT_TRUE(strstr(message, "rewrite URL regex") != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, "[MITM]\nhostname = example.com\n"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(line, 0);
	ANIXOPS_EXPECT_STREQ(message, "ok");

	anixops_engine_free(engine);
}

static void quoted_replacement_is_parsed_as_one_token(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_rewrite_rule(engine, "^https://quote\\.test \"https://target.test/path?a=1&b=two\" 302"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://quote.test", ANIXOPS_PHASE_REQUEST, &result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(result.value, "https://target.test/path?a=1&b=two");

	anixops_engine_free(engine);
}

void anixops_register_config_tests(anixops_test_case_t *tests, size_t *count, size_t cap)
{
	add_test(tests, count, cap, "config/config_parses_rewrite_and_mitm_sections", config_parses_rewrite_and_mitm_sections);
	add_test(tests, count, cap, "config/config_accepts_section_aliases_and_crlf", config_accepts_section_aliases_and_crlf);
	add_test(
		tests,
		count,
		cap,
		"config/config_accepts_header_rewrite_section_aliases",
		config_accepts_header_rewrite_section_aliases);
	add_test(
		tests,
		count,
		cap,
		"config/config_accepts_quantumultx_rewrite_remote_section_aliases",
		config_accepts_quantumultx_rewrite_remote_section_aliases);
	add_test(
		tests,
		count,
		cap,
		"config/config_accepts_h2_enable_mitm_key_alias",
		config_accepts_h2_enable_mitm_key_alias);
	add_test(
		tests,
		count,
		cap,
		"config/config_accepts_disable_quic_mitm_key_aliases",
		config_accepts_disable_quic_mitm_key_aliases);
	add_test(
		tests,
		count,
		cap,
		"config/config_parses_script_and_ignores_unknown_sections",
		config_parses_script_and_ignores_unknown_sections);
	add_test(tests, count, cap, "config/incomplete_rewrite_lines_are_ignored", incomplete_rewrite_lines_are_ignored);
	add_test(tests, count, cap, "config/invalid_regex_is_reported_and_does_not_add_rule", invalid_regex_is_reported_and_does_not_add_rule);
	add_test(tests, count, cap, "config/config_failure_records_line_diagnostic", config_failure_records_line_diagnostic);
	add_test(tests, count, cap, "config/quoted_replacement_is_parsed_as_one_token", quoted_replacement_is_parsed_as_one_token);
}
