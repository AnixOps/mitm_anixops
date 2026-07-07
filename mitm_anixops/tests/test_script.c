#include "test_harness.h"
#include "mitm_anixops.h"

#include <stdlib.h>
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

static char *read_fixture(const char *path)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *data;
	if (file == NULL) {
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	size = ftell(file);
	if (size < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}
	data = (char *)malloc((size_t)size + 1);
	if (data == NULL) {
		fclose(file);
		return NULL;
	}
	if (fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	data[size] = '\0';
	fclose(file);
	return data;
}

static void current_bili_enhanced_plugin_fixture_is_supported(void)
{
	const char *urls[] = {
		"https://app.bilibili.com/x/resource/show/tab/v2?build=1",
		"https://app.biliapi.net/x/v2/account/mine?build=1",
		"https://app.bilibili.com/x/v2/account/mine/ipad?build=1",
		"https://app.biliapi.net/x/v2/region/index?build=1",
		"https://app.bilibili.com/x/v2/channel/region/list?build=1",
	};
	char *fixture = read_fixture("tests/fixtures/BiliBili.Enhanced.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 11);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	for (i = 0; i < sizeof(urls) / sizeof(urls[0]); i++) {
		anixops_script_result_t script;
		ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(engine, urls[i], ANIXOPS_PHASE_RESPONSE, &script), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
		ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
		ANIXOPS_EXPECT_TRUE(strstr(script.script_path, "response.bundle.js") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(script.tag, "BiliBili.Enhanced.") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(script.argument, "Home.Switch=true") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(script.argument, "Storage=Argument") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(script.argument, "LogLevel=WARN") != NULL);
	}

	anixops_engine_free(engine);
	free(fixture);
}

static void representative_loon_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Representative.Loon.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.loon.example/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.loon.example/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.loon.example", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.loon.example/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.request");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=loon");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.loon.example/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.response");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.loon.example", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void representative_surge_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Representative.Surge.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 8);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "name");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "#! metadata ignored");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 4, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "requirement");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.surge.example/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 5000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 8192);
	ANIXOPS_EXPECT_STREQ(script.tag, "Surge.Request");
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=true&Mode=surge");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.surge.example/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 1250);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 4096);
	ANIXOPS_EXPECT_STREQ(script.tag, "Surge.Response");
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=\"true\"&Mode=surge");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.surge.example", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void representative_quantumultx_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Representative.QuantumultX.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.qx.example/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/qx-response.js");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.qx.example", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_DICT);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.qx.example/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://new.qx.example/path");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.qx.example", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void plugin_style_script_matches_response_and_resolves_arguments(void)
{
	const char *config =
		"#!name = BiliBili Enhanced\n"
		"[Argument]\n"
		"Home.Switch = switch,true,tag=home,desc=enable home customization\n"
		"Home.Tab = input,\"live,recommend,hottopic\",tag=home,desc=tabs\n"
		"Storage = select,\"Argument\",\"PersistentStore\",\"database\",tag=storage\n"
		"LogLevel = select,\"WARN\",\"OFF\",\"ERROR\",\"INFO\",tag=debug\n"
		"\n"
		"[Script]\n"
		"http-response ^https?:\\/\\/app\\.bili(bili\\.com|api\\.net)\\/x\\/resource\\/show\\/tab\\/v2\\? "
		"requires-body=1, script-path=https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js, "
		"tag=BiliBili.Enhanced.x.resource.show.tab.v2, argument=[{Home.Switch},{Home.Tab},{Storage},{LogLevel}]\n"
		"\n"
		"[MitM]\n"
		"hostname = app.bilibili.com, app.biliapi.net\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://app.bilibili.com/x/resource/show/tab/v2?foo=1",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(script.tag, "BiliBili.Enhanced.x.resource.show.tab.v2");
	ANIXOPS_EXPECT_STREQ(
		script.script_path,
		"https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js");
	ANIXOPS_EXPECT_STREQ(
		script.argument,
		"Home.Switch=true&Home.Tab=live,recommend,hottopic&Storage=Argument&LogLevel=WARN");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(engine, "Home.Tab", "recommend"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://app.biliapi.net/x/resource/show/tab/v2?foo=1",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_STREQ(script.argument, "Home.Switch=true&Home.Tab=recommend&Storage=Argument&LogLevel=WARN");

	ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(
						  engine,
						  "https://app.bilibili.com/x/resource/show/tab/v2?foo=1",
						  ANIXOPS_PHASE_REQUEST,
						  &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "app.biliapi.net:443", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

static void surge_style_script_rule_template_is_supported(void)
{
	const char *line =
		"Example.Rule = type=http-response, pattern=^https:\\/\\/api\\.example\\.test\\/item\\?, "
		"requires-body=1, script-path=https://scripts.example.test/r.js, "
		"argument=Feature=\"{{{Feature}}}\"&Mode={Mode}";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_argument(engine, "Feature = switch,true"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_argument(engine, "Mode = select,\"fast\",\"safe\""), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_script_rule(engine, line), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/item?id=1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.tag, "Example.Rule");
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=\"true\"&Mode=fast");

	anixops_engine_free(engine);
}

static void anixops_snippet_rewrite_script_lines_are_supported(void)
{
	const char *config =
		"#!name = BiliBili Enhanced\n"
		"#[rewrite_local]\n"
		"^https?:\\/\\/app\\.bili(bili\\.com|api\\.net)\\/x\\/resource\\/show\\/tab\\/v2\\? "
		"url script-response-body https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js\n"
		"#[mitm]\n"
		"hostname = app.bilibili.com, app.biliapi.net\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://app.bilibili.com/x/resource/show/tab/v2?build=1",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(
		script.script_path,
		"https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js");

	anixops_engine_free(engine);
}

static void quantumultx_rewrite_local_section_is_supported(void)
{
	const char *config =
		"[rewrite_local]\n"
		"^https://ads\\.example\\.test reject-dict\n"
		"^https://api\\.example\\.test/list url script-response-body https://scripts.example.test/qx.js\n"
		"[mitm]\n"
		"hostname = %APPEND% api.example.test, ads.example.test\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.example.test", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_DICT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/list", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example.test/qx.js");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

static void quantumultx_url_prefixed_request_scripts_are_supported(void)
{
	const char *config =
		"[rewrite_local]\n"
		"^https://api\\.example\\.test/request-body url script-request-body https://scripts.example.test/qx-request-body.js\n"
		"^https://api\\.example\\.test/request-header url script-request-header https://scripts.example.test/qx-request-header.js\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/request-body", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example.test/qx-request-body.js");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/request-header", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example.test/qx-request-header.js");

	anixops_engine_free(engine);
}

static void quantumultx_url_prefixed_response_header_scripts_are_supported(void)
{
	const char *config =
		"[rewrite_local]\n"
		"^https://api\\.example\\.test/response-header url script-response-header https://scripts.example.test/qx-response-header.js\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/response-header", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/response-header", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example.test/qx-response-header.js");

	anixops_engine_free(engine);
}

static void script_scheduling_attributes_are_exposed(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(
			engine,
			"http-response ^https://api\\.script\\.test/v1 requires-body=1, timeout=\"3\", max-size=1234, "
			"script-path=https://scripts.example/response.js, tag=response.scheduled"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(
			engine,
			"^https://api\\.script\\.test/qx url script-response-body https://scripts.example/qx.js "
			"timeout-ms=250, max_size=512"),
		ANIXOPS_OK);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.script.test/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 3000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 1234);
	ANIXOPS_EXPECT_STREQ(script.tag, "response.scheduled");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.script.test/qx", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 250);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 512);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/qx.js");

	anixops_engine_free(engine);
}

static void sgmodule_inline_arguments_are_supported(void)
{
	const char *config =
		"#!arguments = Feature:true,Mode:\"fast,safe\"\n"
		"#!arguments-desc = Feature: this description must not override Feature\n"
		"[Script]\n"
		"Example.Rule = type=http-response, pattern=^https:\\/\\/api\\.example\\.test\\/item\\?, "
		"requires-body=1, script-path=https://scripts.example.test/r.js, "
		"argument=Feature=\"{{{Feature}}}\"&Mode={Mode}\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.example.test/item?id=1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=\"true\"&Mode=fast,safe");

	anixops_engine_free(engine);
}

static void malformed_and_non_http_script_rules_are_ignored_or_rejected(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_script_rule(engine, "cron \"0 * * * *\" script-path=https://x.test/a.js"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "= type=http-response, pattern=^https:\\/\\/api\\.test, script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "MissingPattern = type=http-response, script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "MissingScript = type=http-response, pattern=^https:\\/\\/api\\.test"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "MalformedAttr = type=http-response, pattern=^https:\\/\\/api\\.test, script-path"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-response [ requires-body=1, script-path=https://x.test/a.js"),
		ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);

	anixops_engine_free(engine);
}

static void no_script_match_returns_none(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_script_rule(engine, "http-request ^https://api\\.example\\.test script-path=https://x.test/a.js"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_script_evaluate_url(engine, "https://other.example.test", ANIXOPS_PHASE_REQUEST, &script), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);
	ANIXOPS_EXPECT_STREQ(script.message, "no script matched");

	anixops_engine_free(engine);
}

void anixops_register_script_tests(anixops_test_case_t *tests, size_t *count, size_t cap)
{
	add_test(
		tests,
		count,
		cap,
		"script/current_bili_enhanced_plugin_fixture_is_supported",
		current_bili_enhanced_plugin_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"script/representative_loon_fixture_is_supported",
		representative_loon_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"script/representative_surge_fixture_is_supported",
		representative_surge_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"script/representative_quantumultx_fixture_is_supported",
		representative_quantumultx_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"script/plugin_style_script_matches_response_and_resolves_arguments",
		plugin_style_script_matches_response_and_resolves_arguments);
	add_test(tests, count, cap, "script/surge_style_script_rule_template_is_supported", surge_style_script_rule_template_is_supported);
	add_test(tests, count, cap, "script/anixops_snippet_rewrite_script_lines_are_supported", anixops_snippet_rewrite_script_lines_are_supported);
	add_test(tests, count, cap, "script/quantumultx_rewrite_local_section_is_supported", quantumultx_rewrite_local_section_is_supported);
	add_test(
		tests,
		count,
		cap,
		"script/quantumultx_url_prefixed_request_scripts_are_supported",
		quantumultx_url_prefixed_request_scripts_are_supported);
	add_test(
		tests,
		count,
		cap,
		"script/quantumultx_url_prefixed_response_header_scripts_are_supported",
		quantumultx_url_prefixed_response_header_scripts_are_supported);
	add_test(tests, count, cap, "script/script_scheduling_attributes_are_exposed", script_scheduling_attributes_are_exposed);
	add_test(tests, count, cap, "script/sgmodule_inline_arguments_are_supported", sgmodule_inline_arguments_are_supported);
	add_test(
		tests,
		count,
		cap,
		"script/malformed_and_non_http_script_rules_are_ignored_or_rejected",
		malformed_and_non_http_script_rules_are_ignored_or_rejected);
	add_test(tests, count, cap, "script/no_script_match_returns_none", no_script_match_returns_none);
}
