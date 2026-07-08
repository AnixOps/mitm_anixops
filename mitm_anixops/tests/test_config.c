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

static void loon_common_fields_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.CommonFields.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	size_t diagnostic_count;
	size_t accepted = 0;
	size_t ignored = 0;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	for (i = 0; i < diagnostic_count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED) {
			accepted++;
		}
		else if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED) {
			ignored++;
		}
	}
	ANIXOPS_EXPECT_TRUE(accepted >= 6);
	ANIXOPS_EXPECT_TRUE(ignored >= 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.common.loon.test/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.common.loon.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.common.loon.test/v1",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Loon-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "prod");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.common.request");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=prod");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.loon.test/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.common.response");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.common.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.common.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_common_fields_strict_fixture_rejects_malformed_rule(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.CommonFields.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "strict compatibility profile") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "strict compatibility profile") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_hashbang_metadata_fixture_records_tolerated_keys(void)
{
	static const char *expected_actions[] = {
		"name",
		"desc",
		"description",
		"author",
		"category",
		"icon",
		"homepage",
		"date",
		"system",
		"requirement",
		"arguments-desc",
	};
	char *fixture = read_fixture("tests/fixtures/Loon.HashbangMetadata.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	size_t diagnostic_count;
	size_t ignored = 0;
	size_t accepted = 0;
	size_t i;
	size_t action_index;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic_count, 13);
	for (i = 0; i < diagnostic_count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED) {
			ignored++;
			ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
			ANIXOPS_EXPECT_STREQ(diagnostic.message, "#! metadata ignored");
		}
		else if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED) {
			accepted++;
		}
	}
	ANIXOPS_EXPECT_EQ_SIZE(ignored, 11);
	ANIXOPS_EXPECT_EQ_SIZE(accepted, 2);

	for (action_index = 0; action_index < sizeof(expected_actions) / sizeof(expected_actions[0]); action_index++) {
		int found = 0;
		for (i = 0; i < diagnostic_count; i++) {
			anixops_rule_diagnostic_t diagnostic;
			ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
			if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED &&
				strcmp(diagnostic.action, expected_actions[action_index]) == 0) {
				found = 1;
			}
		}
		ANIXOPS_EXPECT_TRUE(found);
	}

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_hashbang_metadata_unsupported_keys_are_not_claimed(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.HashbangMetadata.Unsupported.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_common_config_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.CommonConfig.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 4);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.common.qx.test/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.common.qx.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.common.qx.test", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_DICT);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.common.qx.test/v1",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-QX-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "prod");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.qx.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/qx-common-request.js");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.qx.test/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/qx-common-response-header.js");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.common.qx.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "force.common.qx.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_common_config_strict_fixture_rejects_malformed_rule(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.CommonConfig.Malformed.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_QUANTUMULTX_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_QUANTUMULTX_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "strict compatibility profile") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "strict compatibility profile") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_common_config_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.CommonConfig.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	size_t diagnostic_count;
	size_t accepted = 0;
	size_t ignored = 0;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 3);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	for (i = 0; i < diagnostic_count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED) {
			accepted++;
		}
		else if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED) {
			ignored++;
		}
	}
	ANIXOPS_EXPECT_TRUE(accepted >= 6);
	ANIXOPS_EXPECT_TRUE(ignored >= 4);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.common.surge.test/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.common.surge.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.common.surge.test", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.surge.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 3000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 8192);
	ANIXOPS_EXPECT_STREQ(script.tag, "Surge.Common.Request");
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=true&Mode=surge");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://api.common.surge.test/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 1200);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 4096);
	ANIXOPS_EXPECT_STREQ(script.tag, "Surge.Common.Response");
	ANIXOPS_EXPECT_STREQ(script.argument, "Feature=\"true\"&Mode=surge");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.common.surge.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.common.surge.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_common_config_strict_fixture_rejects_malformed_rule(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.CommonConfig.Malformed.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_SURGE_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "strict compatibility profile") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "strict compatibility profile") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void request_rewrite_common_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/RequestRewrite.Common.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.request.rewrite.test/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.request.rewrite.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://seeother.request.rewrite.test/next",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_303);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 303);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.request.rewrite.test/next");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://ads.request.rewrite.test", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "http://old.request.rewrite.test/path", ANIXOPS_PHASE_RESPONSE, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void request_rewrite_common_strict_fixture_rejects_malformed_rule(void)
{
	char *fixture = read_fixture("tests/fixtures/RequestRewrite.Common.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "strict compatibility profile") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "strict compatibility profile") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void policy_intent_common_fixture_covers_reject_subset(void)
{
	char *fixture = read_fixture("tests/fixtures/PolicyIntent.Common.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/reject", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/gone", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 410);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/ok", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/image", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 3);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/reject", ANIXOPS_PHASE_RESPONSE, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void policy_intent_unsupported_routes_are_ignored(void)
{
	char *fixture = read_fixture("tests/fixtures/PolicyIntent.Unsupported.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite rule ignored") != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite rule ignored") != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/direct", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://policy.intent.test/proxy", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void header_mutation_common_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/HeaderMutation.Common.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_header_rewrite_result_t header;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 5);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/add/request",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Trace");
	ANIXOPS_EXPECT_STREQ(header.value, "trace-request");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/replace/prod",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_REPLACE);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "mode-prod");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/drop",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_HEADER_DEL);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Drop");
	ANIXOPS_EXPECT_STREQ(header.value, "");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/response",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"old=Fast",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "new=Fast");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/cookie",
			ANIXOPS_PHASE_RESPONSE,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_DEL);
	ANIXOPS_EXPECT_STREQ(header.header_name, "Set-Cookie");
	ANIXOPS_EXPECT_STREQ(header.value, "");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.header.mutation.test/add/request",
			ANIXOPS_PHASE_RESPONSE,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(header.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void header_mutation_common_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/HeaderMutation.Common.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite header regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "rewrite header regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void response_rewrite_common_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/ResponseRewrite.Common.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	char body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.response.rewrite.test/mock/item",
			ANIXOPS_PHASE_REQUEST,
			"original",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "original");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.response.rewrite.test/mock/item",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "{\"mock\":\"item\"}");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.response.rewrite.test/echo/path",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "echo-path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.response.rewrite.test/clean",
			ANIXOPS_PHASE_RESPONSE,
			"ad=1&ok=1",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "clean=1&ok=1");

	anixops_engine_free(engine);
	free(fixture);
}

static void response_rewrite_common_fixture_rejects_invalid_body_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/ResponseRewrite.Common.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite body regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "rewrite body regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void body_mutation_common_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/BodyMutation.Common.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	char body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.body.mutation.test/request",
			ANIXOPS_PHASE_REQUEST,
			"token=abc",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "token=abc-ok");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.body.mutation.test/request",
			ANIXOPS_PHASE_RESPONSE,
			"token=abc",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_STREQ(body, "token=abc");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.body.mutation.test/response",
			ANIXOPS_PHASE_RESPONSE,
			"ad=1&ok=1",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_STREQ(body, "clean=1&ok=1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.body.mutation.test/json",
			ANIXOPS_PHASE_REQUEST,
			"{\"enabled\":false,\"name\":\"old\"}",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE);
	ANIXOPS_EXPECT_STREQ(body, "{\"enabled\":true,\"name\":\"old\"}");

	anixops_engine_free(engine);
	free(fixture);
}

static void body_mutation_common_fixture_rejects_invalid_body_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/BodyMutation.Common.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite body regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "rewrite body regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void decision_trace_schema_fixture_covers_policy_fields(void)
{
	char *fixture = read_fixture("tests/fixtures/DecisionTrace.Schema.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t mitm;
	anixops_rewrite_result_t rewrite;
	anixops_rewrite_plan_t plan;
	char body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "trace.schema.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_ALLOWED);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "trace.schema.test");
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm allowed");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://trace.schema.test/redirect",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 302);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(rewrite.matched_pattern, "^https:\\/\\/trace\\.schema\\.test\\/redirect");
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://trace.schema.test/new");
	ANIXOPS_EXPECT_STREQ(rewrite.message, "rewrite matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://trace.schema.test/reject", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);
	ANIXOPS_EXPECT_STREQ(rewrite.matched_pattern, "^https:\\/\\/trace\\.schema\\.test\\/reject");
	ANIXOPS_EXPECT_STREQ(rewrite.value, "");
	ANIXOPS_EXPECT_STREQ(rewrite.message, "rewrite matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://trace.schema.test/header",
			ANIXOPS_PHASE_REQUEST,
			NULL,
			NULL,
			0,
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, -1);
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, ANIXOPS_REWRITE_HEADER_ADD);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, 3);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].matched_pattern, "^https:\\/\\/trace\\.schema\\.test\\/header");
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, "X-Trace");
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, "trace-ok");
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].message, "header rewrite matched");
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.script.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.timeout_ms, 1000);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.max_size, 2048);
	ANIXOPS_EXPECT_EQ_INT(plan.script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(plan.script.matched_pattern, "^https:\\/\\/trace\\.schema\\.test\\/header");
	ANIXOPS_EXPECT_STREQ(plan.script.script_path, "https://scripts.test/trace.js");
	ANIXOPS_EXPECT_STREQ(plan.script.tag, "trace.request");
	ANIXOPS_EXPECT_STREQ(plan.script.argument, "Mode=trace");
	ANIXOPS_EXPECT_STREQ(plan.script.message, "script matched");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://trace.schema.test/body",
			ANIXOPS_PHASE_REQUEST,
			"token=Fast",
			body,
			sizeof(body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, 2);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.matched_pattern, "^https:\\/\\/trace\\.schema\\.test\\/body");
	ANIXOPS_EXPECT_STREQ(body, "token=Fast-ok");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void decision_trace_schema_fixture_ignores_unsupported_policy_intent(void)
{
	char *fixture = read_fixture("tests/fixtures/DecisionTrace.Schema.UnsupportedPolicy.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite rule ignored") != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite rule ignored") != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(engine, "https://trace.schema.test/direct", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void plan_api_parity_fixture_matches_legacy_evaluation(void)
{
	char *fixture = read_fixture("tests/fixtures/PlanApiParity.Golden.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_plan_t plan;
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char plan_body[128];
	char legacy_body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 6);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.plan.test/redirect/item",
			ANIXOPS_PHASE_REQUEST,
			NULL,
			NULL,
			0,
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://parity.plan.test/redirect/item",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.status_code, rewrite.status_code);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, rewrite.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.matched_pattern, rewrite.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.value, rewrite.value);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.message, rewrite.message);
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			plan_body,
			sizeof(plan_body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			"token=42",
			legacy_body,
			sizeof(legacy_body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, rewrite.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.matched_pattern, rewrite.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.message, rewrite.message);
	ANIXOPS_EXPECT_STREQ(plan_body, legacy_body);
	ANIXOPS_EXPECT_STREQ(plan_body, "token=42-ok");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 2);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, header.action);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, header.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].matched_pattern, header.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, header.header_name);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, header.value);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].message, header.message);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Trace");
	ANIXOPS_EXPECT_STREQ(header.value, "trace-item");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			(size_t)header.rule_index + 1,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].action, header.action);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[1].rule_index, header.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].matched_pattern, header.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].header_name, header.header_name);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].value, header.value);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[1].message, header.message);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Mode");
	ANIXOPS_EXPECT_STREQ(header.value, "mode-item");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			(size_t)header.rule_index + 1,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://parity.plan.test/request/item",
			ANIXOPS_PHASE_REQUEST,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, script.kind);
	ANIXOPS_EXPECT_EQ_INT(plan.script.phase, script.phase);
	ANIXOPS_EXPECT_EQ_INT(plan.script.requires_body, script.requires_body);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.timeout_ms, script.timeout_ms);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.max_size, script.max_size);
	ANIXOPS_EXPECT_EQ_INT(plan.script.rule_index, script.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.script.matched_pattern, script.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.script.script_path, script.script_path);
	ANIXOPS_EXPECT_STREQ(plan.script.tag, script.tag);
	ANIXOPS_EXPECT_STREQ(plan.script.argument, script.argument);
	ANIXOPS_EXPECT_STREQ(plan.script.message, script.message);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 250);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 1024);
	ANIXOPS_EXPECT_STREQ(script.tag, "parity.request");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.plan.test/response/item",
			ANIXOPS_PHASE_RESPONSE,
			"ad=1&ok=1",
			plan_body,
			sizeof(plan_body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://parity.plan.test/response/item",
			ANIXOPS_PHASE_RESPONSE,
			"ad=1&ok=1",
			legacy_body,
			sizeof(legacy_body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, rewrite.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.matched_pattern, rewrite.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.rewrite.message, rewrite.message);
	ANIXOPS_EXPECT_STREQ(plan_body, legacy_body);
	ANIXOPS_EXPECT_STREQ(plan_body, "clean=1&ok=1");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/response/item",
			ANIXOPS_PHASE_RESPONSE,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, header.action);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, header.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].matched_pattern, header.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, header.header_name);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, header.value);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].message, header.message);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Resp");
	ANIXOPS_EXPECT_STREQ(header.value, "resp-item");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://parity.plan.test/response/item",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, script.kind);
	ANIXOPS_EXPECT_EQ_INT(plan.script.phase, script.phase);
	ANIXOPS_EXPECT_EQ_INT(plan.script.requires_body, script.requires_body);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.timeout_ms, script.timeout_ms);
	ANIXOPS_EXPECT_EQ_SIZE(plan.script.max_size, script.max_size);
	ANIXOPS_EXPECT_EQ_INT(plan.script.rule_index, script.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.script.matched_pattern, script.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.script.script_path, script.script_path);
	ANIXOPS_EXPECT_STREQ(plan.script.tag, script.tag);
	ANIXOPS_EXPECT_STREQ(plan.script.argument, script.argument);
	ANIXOPS_EXPECT_STREQ(plan.script.message, script.message);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 500);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 2048);
	ANIXOPS_EXPECT_STREQ(script.tag, "parity.response");

	anixops_engine_free(engine);
	free(fixture);
}

static void plan_api_parity_fixture_keeps_phase_mismatches_empty(void)
{
	char *fixture = read_fixture("tests/fixtures/PlanApiParity.PhaseMismatch.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_plan_t plan;
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char plan_body[128];
	char legacy_body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.phase.test/request",
			ANIXOPS_PHASE_RESPONSE,
			"token=7",
			plan_body,
			sizeof(plan_body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://parity.phase.test/request",
			ANIXOPS_PHASE_RESPONSE,
			"token=7",
			legacy_body,
			sizeof(legacy_body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, -1);
	ANIXOPS_EXPECT_STREQ(plan_body, legacy_body);
	ANIXOPS_EXPECT_STREQ(plan_body, "token=7");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.phase.test/request",
			ANIXOPS_PHASE_RESPONSE,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://parity.phase.test/request",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.phase.test/response",
			ANIXOPS_PHASE_REQUEST,
			"ad=1",
			plan_body,
			sizeof(plan_body),
			NULL,
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://parity.phase.test/response",
			ANIXOPS_PHASE_REQUEST,
			"ad=1",
			legacy_body,
			sizeof(legacy_body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, rewrite.action);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.rule_index, -1);
	ANIXOPS_EXPECT_STREQ(plan_body, legacy_body);
	ANIXOPS_EXPECT_STREQ(plan_body, "ad=1");
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.phase.test/response",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(header.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://parity.phase.test/response",
			ANIXOPS_PHASE_REQUEST,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void mitm_hostname_malformed_fixture_rejects_invalid_host(void)
{
	char *fixture = read_fixture("tests/fixtures/MITM.Hostname.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "invalid mitm hostname") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "invalid mitm hostname") != NULL);

	anixops_engine_free(engine);
	free(fixture);
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

static void config_accepts_body_rewrite_section_aliases(void)
{
	const char *config =
		"[Body Rewrite]\n"
		"^https://api\\.test/request request-body-replace-regex token=[0-9]+ token=ok\n"
		"[Remote Body Rewrite]\n"
		"^https://api\\.test/response response-body-replace-regex ad clean\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t result;
	char body[128];
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/request",
			ANIXOPS_PHASE_REQUEST,
			"token=123&ok=1",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "token=ok&ok=1");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://api.test/response",
			ANIXOPS_PHASE_RESPONSE,
			"ad=1&ok=1",
			body,
			sizeof(body),
			&result),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(result.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	ANIXOPS_EXPECT_STREQ(body, "clean=1&ok=1");

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

static void config_exposes_skip_server_cert_verify(void)
{
	const char *config =
		"[MITM]\n"
		"hostname = api.example.test\n"
		"skip-server-cert-verify = true\n";
	anixops_engine_t *engine = anixops_engine_new();
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);

	anixops_engine_free(engine);
}

static void config_accepts_quantumultx_mitm_host_options(void)
{
	const char *config =
		"#[mitm]\n"
		"enable = true\n"
		"force-http-engine-hosts = %APPEND% forced.example.test, *.engine.example.test\n"
		"skip-server-cert-verify = forced.example.test, cert.example.test\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "forced.example.test", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.engine.example.test", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_clear(engine);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_load_config(engine, "[MITM]\nskip_server_cert_verify = false\n"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);

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

static void config_records_rule_diagnostics_for_accepted_and_ignored_lines(void)
{
	const char *config =
		"[General]\n"
		"hostname = ignored.test\n"
		"[Rewrite]\n"
		"^https://ok\\.test https://target.test 302\n"
		"^https://incomplete\\.test\n"
		"[MITM]\n"
		"hostname = api.example.test\n"
		"ca-p12 = ignored.p12\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 6);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_PORTABLE);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "section");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 5, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 8);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "ca-p12");

	anixops_engine_free(engine);
}

static void strict_profiles_reject_ignored_rules_in_supported_sections(void)
{
	struct strict_case {
		anixops_compat_profile_t profile;
		const char *config;
		const char *section;
		const char *action;
	} cases[] = {
		{
			ANIXOPS_COMPAT_LOON_STRICT,
			"[Rewrite]\n"
			"^https://incomplete\\.test\n",
			"Rewrite",
			"rewrite",
		},
		{
			ANIXOPS_COMPAT_SURGE_STRICT,
			"[Script]\n"
			"MissingPattern = type=http-response, script-path=https://x.test/a.js\n",
			"Script",
			"script",
		},
		{
			ANIXOPS_COMPAT_QUANTUMULTX_STRICT,
			"[Argument]\n"
			"MissingEquals\n",
			"Argument",
			"argument",
		},
		{
			ANIXOPS_COMPAT_LOON_STRICT,
			"[MITM]\n"
			"ca-p12 = ignored.p12\n",
			"MITM",
			"ca-p12",
		},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		anixops_engine_t *engine = anixops_engine_new();
		anixops_rule_diagnostic_t diagnostic;
		int status = 0;
		size_t line = 0;
		char message[ANIXOPS_MESSAGE_CAP];
		ANIXOPS_EXPECT_TRUE(engine != NULL);

		ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, cases[i].profile), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, cases[i].config), ANIXOPS_ERR_PARSE);
		ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, cases[i].profile);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, cases[i].section);
		ANIXOPS_EXPECT_STREQ(diagnostic.action, cases[i].action);
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "strict compatibility profile") != NULL);
		ANIXOPS_EXPECT_EQ_INT(
			anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
			ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
		ANIXOPS_EXPECT_EQ_SIZE(line, 2);
		ANIXOPS_EXPECT_TRUE(strstr(message, "strict compatibility profile") != NULL);

		anixops_engine_free(engine);
	}
}

static void plugin_metadata_section_is_tolerated_with_diagnostics(void)
{
	const char *config =
		"[Plugin]\n"
		"name = Example Plugin\n"
		"desc = Metadata only\n"
		"[MITM]\n"
		"hostname = api.example.test\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "plugin metadata") != NULL);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");

	anixops_engine_free(engine);
}

static void config_records_rejected_rule_diagnostic_before_returning_error(void)
{
	const char *config =
		"[Rewrite]\n"
		"[ https://target.test 302\n";
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, config), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "rewrite URL regex") != NULL);

	anixops_engine_free(engine);
}

static void pcre2_only_regex_features_report_backend_requirement(void)
{
	struct regex_case {
		const char *config;
		const char *section;
		const char *action;
		const char *context;
		const char *feature;
	} cases[] = {
		{
			"[Rewrite]\n"
			"^https://api\\.test/(?=item)item https://dest.test 302\n",
			"Rewrite",
			"rewrite",
			"rewrite URL regex",
			"lookahead",
		},
		{
			"[Rewrite]\n"
			"^https://body\\.test request-body-replace-regex \"\\p{L}+\" letters\n",
			"Rewrite",
			"rewrite",
			"rewrite body regex",
			"unicode property",
		},
		{
			"[Rewrite]\n"
			"^https://header\\.test response-header-replace-regex X-Test \"\\d++\" digits\n",
			"Rewrite",
			"rewrite",
			"rewrite header regex",
			"possessive quantifier",
		},
		{
			"[Script]\n"
			"http-response ^https://script\\.test/(?<name>item)-\\k<name> script-path=https://x.test/a.js\n",
			"Script",
			"script",
			"script URL regex",
			"named backreference",
		},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		anixops_engine_t *engine = anixops_engine_new();
		anixops_rule_diagnostic_t diagnostic;
		int status = 0;
		size_t line = 0;
		char message[ANIXOPS_MESSAGE_CAP];
		ANIXOPS_EXPECT_TRUE(engine != NULL);

		ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, cases[i].config), ANIXOPS_ERR_REGEX);
		ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, cases[i].section);
		ANIXOPS_EXPECT_STREQ(diagnostic.action, cases[i].action);
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, cases[i].context) != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "requires pcre2 backend") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, cases[i].feature) != NULL);

		ANIXOPS_EXPECT_EQ_INT(
			anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
			ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
		ANIXOPS_EXPECT_EQ_SIZE(line, 2);
		ANIXOPS_EXPECT_TRUE(strstr(message, cases[i].context) != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(message, "requires pcre2 backend") != NULL);
		ANIXOPS_EXPECT_TRUE(strstr(message, cases[i].feature) != NULL);

		anixops_engine_free(engine);
	}
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
	add_test(tests, count, cap, "config/loon_common_fields_fixture_is_supported", loon_common_fields_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/loon_common_fields_strict_fixture_rejects_malformed_rule",
		loon_common_fields_strict_fixture_rejects_malformed_rule);
	add_test(
		tests,
		count,
		cap,
		"config/loon_hashbang_metadata_fixture_records_tolerated_keys",
		loon_hashbang_metadata_fixture_records_tolerated_keys);
	add_test(
		tests,
		count,
		cap,
		"config/loon_hashbang_metadata_unsupported_keys_are_not_claimed",
		loon_hashbang_metadata_unsupported_keys_are_not_claimed);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_common_config_fixture_is_supported",
		quantumultx_common_config_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_common_config_strict_fixture_rejects_malformed_rule",
		quantumultx_common_config_strict_fixture_rejects_malformed_rule);
	add_test(
		tests,
		count,
		cap,
		"config/surge_common_config_fixture_is_supported",
		surge_common_config_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/surge_common_config_strict_fixture_rejects_malformed_rule",
		surge_common_config_strict_fixture_rejects_malformed_rule);
	add_test(
		tests,
		count,
		cap,
		"config/request_rewrite_common_fixture_is_supported",
		request_rewrite_common_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/request_rewrite_common_strict_fixture_rejects_malformed_rule",
		request_rewrite_common_strict_fixture_rejects_malformed_rule);
	add_test(
		tests,
		count,
		cap,
		"config/policy_intent_common_fixture_covers_reject_subset",
		policy_intent_common_fixture_covers_reject_subset);
	add_test(
		tests,
		count,
		cap,
		"config/policy_intent_unsupported_routes_are_ignored",
		policy_intent_unsupported_routes_are_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/header_mutation_common_fixture_is_supported",
		header_mutation_common_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/header_mutation_common_fixture_rejects_invalid_regex",
		header_mutation_common_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/response_rewrite_common_fixture_is_supported",
		response_rewrite_common_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/response_rewrite_common_fixture_rejects_invalid_body_regex",
		response_rewrite_common_fixture_rejects_invalid_body_regex);
	add_test(
		tests,
		count,
		cap,
		"config/body_mutation_common_fixture_is_supported",
		body_mutation_common_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/body_mutation_common_fixture_rejects_invalid_body_regex",
		body_mutation_common_fixture_rejects_invalid_body_regex);
	add_test(
		tests,
		count,
		cap,
		"config/decision_trace_schema_fixture_covers_policy_fields",
		decision_trace_schema_fixture_covers_policy_fields);
	add_test(
		tests,
		count,
		cap,
		"config/decision_trace_schema_fixture_ignores_unsupported_policy_intent",
		decision_trace_schema_fixture_ignores_unsupported_policy_intent);
	add_test(
		tests,
		count,
		cap,
		"config/plan_api_parity_fixture_matches_legacy_evaluation",
		plan_api_parity_fixture_matches_legacy_evaluation);
	add_test(
		tests,
		count,
		cap,
		"config/plan_api_parity_fixture_keeps_phase_mismatches_empty",
		plan_api_parity_fixture_keeps_phase_mismatches_empty);
	add_test(
		tests,
		count,
		cap,
		"config/mitm_hostname_malformed_fixture_rejects_invalid_host",
		mitm_hostname_malformed_fixture_rejects_invalid_host);
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
		"config/config_accepts_body_rewrite_section_aliases",
		config_accepts_body_rewrite_section_aliases);
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
		"config/config_exposes_skip_server_cert_verify",
		config_exposes_skip_server_cert_verify);
	add_test(
		tests,
		count,
		cap,
		"config/config_accepts_quantumultx_mitm_host_options",
		config_accepts_quantumultx_mitm_host_options);
	add_test(
		tests,
		count,
		cap,
		"config/config_parses_script_and_ignores_unknown_sections",
		config_parses_script_and_ignores_unknown_sections);
	add_test(tests, count, cap, "config/incomplete_rewrite_lines_are_ignored", incomplete_rewrite_lines_are_ignored);
	add_test(tests, count, cap, "config/invalid_regex_is_reported_and_does_not_add_rule", invalid_regex_is_reported_and_does_not_add_rule);
	add_test(tests, count, cap, "config/config_failure_records_line_diagnostic", config_failure_records_line_diagnostic);
	add_test(
		tests,
		count,
		cap,
		"config/config_records_rule_diagnostics_for_accepted_and_ignored_lines",
		config_records_rule_diagnostics_for_accepted_and_ignored_lines);
	add_test(
		tests,
		count,
		cap,
		"config/strict_profiles_reject_ignored_rules_in_supported_sections",
		strict_profiles_reject_ignored_rules_in_supported_sections);
	add_test(
		tests,
		count,
		cap,
		"config/plugin_metadata_section_is_tolerated_with_diagnostics",
		plugin_metadata_section_is_tolerated_with_diagnostics);
	add_test(
		tests,
		count,
		cap,
		"config/config_records_rejected_rule_diagnostic_before_returning_error",
		config_records_rejected_rule_diagnostic_before_returning_error);
	add_test(
		tests,
		count,
		cap,
		"config/pcre2_only_regex_features_report_backend_requirement",
		pcre2_only_regex_features_report_backend_requirement);
	add_test(tests, count, cap, "config/quoted_replacement_is_parsed_as_one_token", quoted_replacement_is_parsed_as_one_token);
}
