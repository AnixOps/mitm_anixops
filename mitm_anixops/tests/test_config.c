#include "test_harness.h"
#include "mitm_anixops.h"

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
		"config/config_parses_script_and_ignores_unknown_sections",
		config_parses_script_and_ignores_unknown_sections);
	add_test(tests, count, cap, "config/incomplete_rewrite_lines_are_ignored", incomplete_rewrite_lines_are_ignored);
	add_test(tests, count, cap, "config/invalid_regex_is_reported_and_does_not_add_rule", invalid_regex_is_reported_and_does_not_add_rule);
	add_test(tests, count, cap, "config/quoted_replacement_is_parsed_as_one_token", quoted_replacement_is_parsed_as_one_token);
}
