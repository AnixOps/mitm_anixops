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

static void loon_mitm_options_fixture_exposes_adapter_flags(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.MitmOptions.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 3);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "enable");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm option accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm hostname accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "skip-server-cert-verify");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm option accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.options.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "api.options.loon.test");
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "sub.wild.options.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "*.wild.options.loon.test");
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.options.loon.test", 1, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.options.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_mitm_options_malformed_fixture_rejects_invalid_host(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.MitmOptions.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "enable");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "invalid mitm hostname pattern") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "invalid mitm hostname pattern") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_mitm_certificate_unsupported_fixture_keeps_material_ignored(void)
{
	static const char *expected_actions[] = {"ca-p12", "ca-passphrase", "ca-cert"};
	char *fixture = read_fixture("tests/fixtures/Loon.MitmCertificateUnsupported.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "enable");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	for (i = 0; i < sizeof(expected_actions) / sizeof(expected_actions[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i + 2, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 4);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, expected_actions[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "unsupported mitm option ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "cert-material.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm certificate not trusted");

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_reject_fixture_maps_domain_suffix_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainReject.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://ads.rule.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.ads.rule.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://media.rule.loon.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://notads.rule.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://ads.rule.loon.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainReject.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN-SUFFIX") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN-SUFFIX") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_exact_reject_fixture_maps_exact_domain_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainExactReject.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://exact.rule.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://media-exact.rule.loon.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.exact.rule.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://exact.rule.loon.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainExactReject.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainKeywordReject.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://api.tracking-loon.rule.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.media-keyword-loon.rule.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://plain.rule.loon.test/tracking-loon",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://api.tracking-loon.rule.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleDomainKeywordReject.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN-KEYWORD") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN-KEYWORD") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_final_reject_fixture_maps_final_reject(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleFinalReject.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://anything.final.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"http://plain.final.loon.test/",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://anything.final.loon.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_final_reject_malformed_fixture_rejects_missing_action(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleFinalReject.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "FINAL") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "FINAL") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_url_regex_reject_fixture_maps_url_regex_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleUrlRegexReject.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.loon.test/ads",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.loon.test/gone",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 410);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.loon.test/open",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.loon.test/ads",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_url_regex_reject_malformed_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.RuleUrlRegexReject.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_rule_route_unsupported_fixture_keeps_routes_ignored(void)
{
	static const size_t expected_lines[] = {3, 4, 5, 6, 7};
	char *fixture = read_fixture("tests/fixtures/Loon.RuleRouteUnsupported.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	for (i = 0; i < sizeof(expected_lines) / sizeof(expected_lines[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, expected_lines[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://route.guard.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://proxy.guard.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://unmatched.guard.loon.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

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

static void loon_argument_section_fixture_resolves_script_defaults(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.ArgumentSection.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	for (i = 0; i < 3; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "argument");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "argument rule accepted");
	}
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 7);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 4, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 10);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://argument.section.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.argument.section");
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/loon-argument-section.js");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=prod&Token=quoted token&Flag=true&Missing=");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(engine, "Token", "override token"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://argument.section.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=prod&Token=override token&Flag=true&Missing=");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "argument.section.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_argument_section_malformed_fixture_rejects_missing_equals(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.ArgumentSection.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "argument");
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

static void loon_script_metadata_fixture_exposes_dispatch_fields(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.ScriptMetadata.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 4);

	for (i = 0; i < 3; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "script rule accepted");
	}
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 7);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.metadata.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 2000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 4096);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/loon-script-request.js");
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.script.request");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=request");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://script.metadata.loon.test/v1", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 750);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 2048);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 1);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/loon-script-response.js");
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.script.response");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=response");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://script.metadata.loon.test/disabled",
			ANIXOPS_PHASE_REQUEST,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "script.metadata.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_script_metadata_malformed_fixture_rejects_missing_path(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.ScriptMetadata.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
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

static void loon_task_metadata_fixture_emits_task_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.TaskMetadata.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	anixops_task_descriptor_t task;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 6);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "argument");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "argument");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 7);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 4, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 8);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "script rule accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 5, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 11);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://task.metadata.loon.test/http", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/loon-http.js");
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.task.http");
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://task.metadata.loon.test/http", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 0, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 5000);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 8192);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "15 8 * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/loon-cron.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "loon.task.cron");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=cron");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-cron");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 1, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_INTERVAL);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 1800);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 1250);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 4096);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "1800");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/loon-interval.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "loon.task.interval");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=interval");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "task.metadata.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_task_metadata_malformed_fixture_rejects_invalid_cron(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.TaskMetadata.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "cron expression") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "cron expression") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_task_unsupported_fixture_keeps_non_task_types_ignored(void)
{
	static const size_t expected_lines[] = {2, 3, 4};
	char *fixture = read_fixture("tests/fixtures/Loon.TaskUnsupported.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	for (i = 0; i < sizeof(expected_lines) / sizeof(expected_lines[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, expected_lines[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "script rule ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://scripts.example/loon-dns.js", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_inline_arguments_fixture_resolves_script_defaults(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.InlineArguments.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "arguments");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "inline arguments accepted");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://inline.args.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 2000);
	ANIXOPS_EXPECT_STREQ(script.tag, "loon.inline.arguments");
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/loon-inline-arguments.js");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=prod&Token=quoted token&Missing=");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_argument_value(engine, "Mode", "override"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://inline.args.loon.test/v1", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=override&Token=quoted token&Missing=");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "inline.args.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_inline_arguments_malformed_fixture_rejects_missing_separator(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.InlineArguments.Malformed.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "arguments");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "missing ':' separator") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 1);
	ANIXOPS_EXPECT_TRUE(strstr(message, "missing ':' separator") != NULL);

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

static void quantumultx_echo_response_fixture_maps_response_body(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.EchoResponse.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	char body[128];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rewrite rule accepted");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_apply_body(
			engine,
			"https://echo.common.qx.test/item",
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
			"https://echo.common.qx.test/item",
			ANIXOPS_PHASE_RESPONSE,
			"original",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_MOCK_RESPONSE_BODY);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "echo-item");
	ANIXOPS_EXPECT_STREQ(body, "echo-item");

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_echo_response_malformed_fixture_rejects_missing_body(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.EchoResponse.Malformed.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_QUANTUMULTX_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
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

static void quantumultx_task_metadata_fixture_emits_task_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.TaskMetadata.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	anixops_task_descriptor_t task;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 4);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "task descriptor accepted");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://scripts.example/qx-task-cron.js", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 0, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 4000);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 2048);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "*/15 * * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/qx-task-cron.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "qx.task.cron");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=cron");
	ANIXOPS_EXPECT_STREQ(task.origin, "quantumultx-task-local-cron");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 1, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 0);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "0 30 8 * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/qx-task-six-field.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "qx.task.six");
	ANIXOPS_EXPECT_STREQ(task.origin, "quantumultx-task-local-cron");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 2, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_EVENT_NETWORK);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 0);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "event-network");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/qx-task-network.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "qx.task.network");
	ANIXOPS_EXPECT_STREQ(task.origin, "quantumultx-task-local-event");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 3, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_EVENT_INTERACTION);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 2500);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 5120);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "event-interaction");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/qx-task-interaction.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "qx.task.interaction");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=event");
	ANIXOPS_EXPECT_STREQ(task.origin, "quantumultx-task-local-event");

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.TaskMetadata.EventMalformed.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_QUANTUMULTX_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_QUANTUMULTX_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "event script path") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "event script path") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_task_metadata_unsupported_event_fixture_stays_ignored(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.TaskMetadata.UnsupportedEvent.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "script rule ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://scripts.example/qx-task-location.js", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.TaskMetadata.Malformed.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_QUANTUMULTX_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_QUANTUMULTX_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "cron expression") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "cron expression") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_mitm_options_fixture_exposes_adapter_flags(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.MitmOptions.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 3);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 6);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "force-http-engine-hosts");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm hostname accepted");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "skip-server-cert-verify");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm option accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.options.qx.test", 1, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "forced.options.qx.test", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "sub.engine.options.qx.test", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_mitm_options_malformed_fixture_rejects_invalid_host(void)
{
	char *fixture = read_fixture("tests/fixtures/QuantumultX.MitmOptions.Malformed.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_QUANTUMULTX_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "enable");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_QUANTUMULTX_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "force-http-engine-hosts");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "invalid mitm hostname pattern") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "invalid mitm hostname pattern") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void quantumultx_mitm_certificate_unsupported_fixture_keeps_options_ignored(void)
{
	static const char *expected_actions[] = {"passphrase", "p12", "skip_validating_cert"};
	char *fixture = read_fixture("tests/fixtures/QuantumultX.MitmCertificateUnsupported.snippet");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "enable");
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	for (i = 0; i < sizeof(expected_actions) / sizeof(expected_actions[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i + 2, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 4);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, expected_actions[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "unsupported mitm option ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "cert-material.qx.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm certificate not trusted");

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

static void surge_mitm_certificate_unsupported_fixture_keeps_material_ignored(void)
{
	static const char *expected_actions[] = {"ca-p12", "ca-passphrase"};
	char *fixture = read_fixture("tests/fixtures/Surge.MitmCertificateUnsupported.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm hostname accepted");

	for (i = 0; i < sizeof(expected_actions) / sizeof(expected_actions[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i + 1, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 3);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, expected_actions[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "unsupported mitm option ignored");
	}

	anixops_engine_set_mitm_enabled(engine, 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "cert-material.surge.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm certificate not trusted");

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_script_unsupported_fixture_keeps_non_task_types_ignored(void)
{
	static const size_t expected_lines[] = {2, 3, 4};
	char *fixture = read_fixture("tests/fixtures/Surge.ScriptUnsupported.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	for (i = 0; i < sizeof(expected_lines) / sizeof(expected_lines[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, expected_lines[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "script rule ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://scripts.example/surge-dns.js", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_task_metadata_fixture_emits_task_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.TaskMetadata.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	anixops_task_descriptor_t task;
	size_t diagnostic_count;
	size_t accepted_arguments = 0;
	size_t accepted_scripts = 0;
	size_t accepted_tasks = 0;
	size_t ignored_metadata = 0;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic_count, 6);
	for (i = 0; i < diagnostic_count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED) {
			if (strcmp(diagnostic.action, "arguments") == 0) {
				accepted_arguments++;
				ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
				ANIXOPS_EXPECT_STREQ(diagnostic.section, "Argument");
			}
			else if (strcmp(diagnostic.action, "task") == 0) {
				accepted_tasks++;
				ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
				ANIXOPS_EXPECT_TRUE(
					diagnostic.line == 4 || diagnostic.line == 5 || diagnostic.line == 6);
				ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "task descriptor accepted") != NULL);
			}
			else if (strcmp(diagnostic.action, "script") == 0) {
				accepted_scripts++;
				ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 7);
				ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
				ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "script rule accepted") != NULL);
			}
		}
		else if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED) {
			ignored_metadata++;
			ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
			ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
			ANIXOPS_EXPECT_STREQ(diagnostic.action, "name");
		}
	}
	ANIXOPS_EXPECT_EQ_SIZE(accepted_arguments, 1);
	ANIXOPS_EXPECT_EQ_SIZE(accepted_scripts, 1);
	ANIXOPS_EXPECT_EQ_SIZE(accepted_tasks, 3);
	ANIXOPS_EXPECT_EQ_SIZE(ignored_metadata, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://task.surge.example/http", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/surge-task-http.js");
	ANIXOPS_EXPECT_STREQ(script.tag, "surge.task.http");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://scripts.example/surge-task-cron.js",
			ANIXOPS_PHASE_REQUEST,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 0, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 6000);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 2048);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "*/20 * * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/surge-task-cron.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "Surge.Task.Cron");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=surge");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 1, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 0);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "0 15 9 * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/surge-task-six.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "surge.task.six");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 2, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_INTERVAL);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 1800);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 750);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "1800");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/surge-task-interval.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "surge.task.interval");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_task_metadata_malformed_fixture_rejects_invalid_cron(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.TaskMetadata.Malformed.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_SURGE_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "cron expression") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "cron expression") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_task_event_fixture_emits_event_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.TaskEvent.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_task_descriptor_t task;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 0, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_EVENT);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 2750);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 5120);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "network-changed");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/surge-event-network.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "surge.event.network");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=event");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 1, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_EVENT);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 0);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "notification");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.example/surge-event-notification.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "surge.event.notification");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_task_event_malformed_fixture_rejects_missing_event_name(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.TaskEvent.Malformed.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_SURGE_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "event name") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "event name") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_task_event_unsupported_fixture_rejects_unknown_event_name(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.TaskEvent.Unsupported.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_SURGE_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "unsupported task event name") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "unsupported task event name") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_requirement_metadata_fixture_records_tolerated_keys(void)
{
	static const char *expected_actions[] = {
		"name",
		"desc",
		"requirement",
		"system",
		"arguments-desc",
	};
	char *fixture = read_fixture("tests/fixtures/Surge.RequirementMetadata.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t diagnostic_count;
	size_t ignored = 0;
	size_t accepted = 0;
	size_t i;
	size_t action_index;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic_count, 6);
	for (i = 0; i < diagnostic_count; i++) {
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
	ANIXOPS_EXPECT_EQ_SIZE(ignored, 5);
	ANIXOPS_EXPECT_EQ_SIZE(accepted, 1);

	for (action_index = 0; action_index < sizeof(expected_actions) / sizeof(expected_actions[0]); action_index++) {
		int found = 0;
		for (i = 0; i < diagnostic_count; i++) {
			ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
			if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_IGNORED &&
				strcmp(diagnostic.action, expected_actions[action_index]) == 0) {
				found = 1;
			}
		}
		ANIXOPS_EXPECT_TRUE(found);
	}

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 5, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 8);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "requirement.metadata.surge.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void surge_requirement_metadata_unsupported_keys_are_not_claimed(void)
{
	char *fixture = read_fixture("tests/fixtures/Surge.RequirementMetadata.Unsupported.sgmodule");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_SURGE_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "line ignored outside supported section");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_SURGE_STRICT);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

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

static void cron_task_trigger_common_fixture_emits_task_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/CronTaskTrigger.HttpScriptGuard.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_script_result_t script;
	anixops_task_descriptor_t task;
	size_t diagnostic_count;
	size_t accepted_scripts = 0;
	size_t accepted_tasks = 0;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 3);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);

	diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic_count, 4);
	for (i = 0; i < diagnostic_count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
		if (diagnostic.status == ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED) {
			if (strcmp(diagnostic.action, "script") == 0) {
				accepted_scripts++;
				ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
				ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "script rule accepted") != NULL);
			}
			else if (strcmp(diagnostic.action, "task") == 0) {
				accepted_tasks++;
				ANIXOPS_EXPECT_TRUE(diagnostic.line == 3 || diagnostic.line == 4 || diagnostic.line == 5);
				ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "task descriptor accepted") != NULL);
			}
		}
	}
	ANIXOPS_EXPECT_EQ_SIZE(accepted_scripts, 1);
	ANIXOPS_EXPECT_EQ_SIZE(accepted_tasks, 3);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://cron.task.test/http", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 3000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 1024);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, 0);
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.test/http.js");
	ANIXOPS_EXPECT_STREQ(script.tag, "cron.task.http.guard");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://cron.task.test/http", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 0, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 0);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 2000);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 2048);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 1);
	ANIXOPS_EXPECT_STREQ(task.schedule, "0 * * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.test/cron.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "cron.task.common");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=cron");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-cron");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 1, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_CRON);
	ANIXOPS_EXPECT_EQ_SIZE(task.timeout_ms, 1500);
	ANIXOPS_EXPECT_EQ_SIZE(task.max_size, 4096);
	ANIXOPS_EXPECT_STREQ(task.schedule, "0 8 * * *");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.test/task.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "task.common");
	ANIXOPS_EXPECT_STREQ(task.argument, "Mode=task");
	ANIXOPS_EXPECT_STREQ(task.origin, "script-section-attr-list");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_task_descriptor(engine, 2, &task), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(task.kind, ANIXOPS_TASK_INTERVAL);
	ANIXOPS_EXPECT_EQ_SIZE(task.interval_seconds, 3600);
	ANIXOPS_EXPECT_EQ_INT(task.enabled, 0);
	ANIXOPS_EXPECT_STREQ(task.schedule, "3600");
	ANIXOPS_EXPECT_STREQ(task.script_path, "https://scripts.test/interval.js");
	ANIXOPS_EXPECT_STREQ(task.tag, "interval.common");

	anixops_engine_free(engine);
	free(fixture);
}

static void cron_task_trigger_unsupported_fixture_does_not_register_descriptors(void)
{
	char *fixture = read_fixture("tests/fixtures/CronTaskTrigger.Unsupported.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_script_result_t script;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	for (i = 0; i < 3; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "script");
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "script rule ignored") != NULL);
	}

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(engine, "https://cron.task.test/http", ANIXOPS_PHASE_REQUEST, &script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_NONE);
	ANIXOPS_EXPECT_EQ_INT(script.rule_index, -1);

	anixops_engine_free(engine);
	free(fixture);
}

static void cron_task_trigger_malformed_fixture_rejects_invalid_cron(void)
{
	char *fixture = read_fixture("tests/fixtures/CronTaskTrigger.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Script");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "task");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "cron expression") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 2);
	ANIXOPS_EXPECT_TRUE(strstr(message, "cron expression") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_mitm_fixture_exposes_host_patterns(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.HttpMitm.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 4);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 4);

	for (i = 0; i < 4; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 4);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "mitm");
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "stash mitm hostname accepted") != NULL);
	}

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "stash.example.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "stash.example.test");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.stash.example.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "*.stash.example.test");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.stash.example.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "blocked.stash.example.test");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "weather-data.port-stash.test:443", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "weather-data.port-stash.test");

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_mitm_malformed_fixture_rejects_invalid_host(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.HttpMitm.Malformed.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "mitm");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "invalid mitm hostname pattern") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "invalid mitm hostname pattern") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_mitm_port_specific_fixture_stays_unsupported(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.HttpMitm.PortSpecificUnsupported.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "line ignored outside supported section");

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_mitm_certificate_unsupported_fixture_keeps_material_ignored(void)
{
	static const char *expected_actions[] = {"ca", "ca-passphrase"};
	char *fixture = read_fixture("tests/fixtures/Stash.HttpMitmCertificateUnsupported.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	for (i = 0; i < sizeof(expected_actions) / sizeof(expected_actions[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 3);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, expected_actions[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "unsupported stash mitm option ignored");
	}

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "mitm");

	anixops_engine_set_mitm_enabled(engine, 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "cert-material.stash.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm certificate not trusted");

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_force_http_engine_fixture_exposes_quic_signal(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.HttpForceHttpEngine.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	anixops_engine_set_disable_quic_for_mitm(engine, 0);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "force-http-engine");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "stash mitm option accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "mitm");

	anixops_engine_set_mitm_enabled(engine, 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "quic.force-http-engine.stash.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "quic.force-http-engine.stash.test", 1, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_REJECT_QUIC);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_QUIC_DISABLED_FOR_MITM);
	ANIXOPS_EXPECT_STREQ(mitm.matched_pattern, "quic.force-http-engine.stash.test");

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.HttpForceHttpEngine.Malformed.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "force-http-engine");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "force-http-engine boolean") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "force-http-engine boolean") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_url_rewrite_fixture_maps_reject_subset(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.UrlRewrite.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "stash url-rewrite accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "stash url-rewrite ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-rewrite.example.test/ads",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-rewrite.example.test/gone",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 410);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-rewrite.example.test/transparent",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-rewrite.example.test/ads",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_url_rewrite_redirect_fixture_maps_redirect_subset(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.UrlRewriteRedirect.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "stash url-rewrite accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "stash url-rewrite ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"http://stash-redirect.example.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://stash-redirect.example.test/path");
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-redirect.example.test/temp/next",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_307);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 307);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://edge.stash-redirect.example.test/next");
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://stash-redirect.example.test/transparent",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"http://stash-redirect.example.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.UrlRewriteRedirect.Malformed.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 4);
	ANIXOPS_EXPECT_TRUE(strstr(message, "regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_url_rewrite_malformed_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.UrlRewrite.Malformed.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "url-rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 4);
	ANIXOPS_EXPECT_TRUE(strstr(message, "regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void stash_migration_guard_fixture_stays_parser_unsupported(void)
{
	char *fixture = read_fixture("tests/fixtures/Stash.MigrationGuard.yaml");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 7);

	for (i = 0; i < 7; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 2);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");
		ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "line ignored outside supported section") != NULL);
	}

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_migration_guard_fixture_stays_parser_unsupported(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.MigrationGuard.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 5);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 1);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "section");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "unsupported section ignored") != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 2);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 3, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "section");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 4, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 6);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_reject_fixture_maps_url_regex_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleReject.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "shadowrocket rule accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.shadowrocket.test/ads",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.shadowrocket.test/gone",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 410);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://rule.shadowrocket.test/ads",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleReject.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainReject.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "shadowrocket rule accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.media.domain-rule.shadowrocket.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://domain-rule.shadowrocket.test/home",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://not-domain-rule.shadowrocket.test/home",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://domain-rule.shadowrocket.test/home",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainReject.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN-SUFFIX") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN-SUFFIX") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainExactReject.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "shadowrocket rule accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://exact.domain-rule.shadowrocket.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 0);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://media.domain-rule.shadowrocket.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.exact.domain-rule.shadowrocket.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://exact.domain-rule.shadowrocket.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainExactReject.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainKeywordReject.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 3);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "shadowrocket rule accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 2, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 5);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://api.tracking.shadowrocket.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://cdn.media-keyword.shadowrocket.test/image.gif",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_IMG);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://plain.shadowrocket.test/tracking",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, -1);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://api.tracking.shadowrocket.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleDomainKeywordReject.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "DOMAIN-KEYWORD") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "DOMAIN-KEYWORD") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_final_reject_fixture_maps_final_reject(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleFinalReject.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_rewrite_result_t rewrite;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 2);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "shadowrocket rule accepted");

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 1, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "rule line ignored");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://anything.final.shadowrocket.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"http://plain.final.shadowrocket.test/",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.status_code, 200);
	ANIXOPS_EXPECT_EQ_INT(rewrite.rule_index, 0);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://anything.final.shadowrocket.test/path",
			ANIXOPS_PHASE_RESPONSE,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_NONE);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.RuleFinalReject.Malformed.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	int status = 0;
	size_t line = 0;
	char message[ANIXOPS_MESSAGE_CAP];
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_REJECTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rule");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rule");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "FINAL") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_PARSE);
	ANIXOPS_EXPECT_EQ_SIZE(line, 3);
	ANIXOPS_EXPECT_TRUE(strstr(message, "FINAL") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_common_config_fixture_is_supported(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.CommonConfig.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rewrite_result_t rewrite;
	anixops_script_result_t script;
	anixops_mitm_decision_t mitm;
	anixops_rule_diagnostic_t diagnostic;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 2);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 3);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_h2_mitm_enabled(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 7);

	for (i = 0; i < 7; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	}

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"http://old.common.shadowrocket.test/path",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	ANIXOPS_EXPECT_STREQ(rewrite.value, "https://api.common.shadowrocket.test/path");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_url(
			engine,
			"https://ads.common.shadowrocket.test",
			ANIXOPS_PHASE_REQUEST,
			&rewrite),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(rewrite.action, ANIXOPS_REWRITE_REJECT_200);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://api.common.shadowrocket.test/v1",
			ANIXOPS_PHASE_REQUEST,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 1);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 2000);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 4096);
	ANIXOPS_EXPECT_STREQ(script.tag, "Shadowrocket.Common.Request");
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/shadowrocket-common-request.js");
	ANIXOPS_EXPECT_STREQ(script.argument, "Mode=shadowrocket");

	ANIXOPS_EXPECT_EQ_INT(
		anixops_script_evaluate_url(
			engine,
			"https://api.common.shadowrocket.test/v1",
			ANIXOPS_PHASE_RESPONSE,
			&script),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(script.requires_body, 0);
	ANIXOPS_EXPECT_EQ_SIZE(script.timeout_ms, 900);
	ANIXOPS_EXPECT_EQ_SIZE(script.max_size, 2048);
	ANIXOPS_EXPECT_STREQ(script.tag, "Shadowrocket.Common.Response");
	ANIXOPS_EXPECT_STREQ(script.script_path, "https://scripts.example/shadowrocket-common-response.js");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.common.shadowrocket.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.common.shadowrocket.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_common_config_fixture_rejects_invalid_regex(void)
{
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf");
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
	ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_PORTABLE);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 4);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "Rewrite");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "rewrite");
	ANIXOPS_EXPECT_TRUE(strstr(diagnostic.message, "regex") != NULL);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message)),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(status, ANIXOPS_ERR_REGEX);
	ANIXOPS_EXPECT_EQ_SIZE(line, 4);
	ANIXOPS_EXPECT_TRUE(strstr(message, "regex") != NULL);

	anixops_engine_free(engine);
	free(fixture);
}

static void shadowrocket_mitm_certificate_unsupported_fixture_keeps_material_ignored(void)
{
	static const char *expected_actions[] = {"ca-p12", "ca-passphrase", "ca-cert"};
	char *fixture = read_fixture("tests/fixtures/Shadowrocket.MitmCertificateUnsupported.conf");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_task_descriptor_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_skip_server_cert_verify(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 4);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 0, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, 3);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");
	ANIXOPS_EXPECT_STREQ(diagnostic.message, "mitm hostname accepted");

	for (i = 0; i < sizeof(expected_actions) / sizeof(expected_actions[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i + 1, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_SIZE(diagnostic.line, i + 4);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, expected_actions[i]);
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "unsupported mitm option ignored");
	}

	anixops_engine_set_mitm_enabled(engine, 1);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "cert-material.shadowrocket.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(mitm.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
	ANIXOPS_EXPECT_STREQ(mitm.message, "mitm certificate not trusted");

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
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 8);
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
			"https://parity.plan.test/request-current/item",
			ANIXOPS_PHASE_REQUEST,
			NULL,
			NULL,
			0,
			"old-item",
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/request-current/item",
			ANIXOPS_PHASE_REQUEST,
			0,
			"old-item",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_REQUEST);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, header.action);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, header.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].matched_pattern, header.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, header.header_name);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, header.value);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].message, header.message);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Current");
	ANIXOPS_EXPECT_STREQ(header.value, "req-item");
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);

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

	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_build_plan(
			engine,
			"https://parity.plan.test/response-current/item",
			ANIXOPS_PHASE_RESPONSE,
			NULL,
			NULL,
			0,
			"old-item",
			&plan),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(
		anixops_rewrite_evaluate_header(
			engine,
			"https://parity.plan.test/response-current/item",
			ANIXOPS_PHASE_RESPONSE,
			0,
			"old-item",
			&header),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(plan.phase, ANIXOPS_PHASE_RESPONSE);
	ANIXOPS_EXPECT_EQ_INT(plan.body_available, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.requires_body, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.rewrite.action, ANIXOPS_REWRITE_NONE);
	ANIXOPS_EXPECT_EQ_SIZE(plan.header_rewrite_count, 1);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrite_truncated, 0);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].action, header.action);
	ANIXOPS_EXPECT_EQ_INT(plan.header_rewrites[0].rule_index, header.rule_index);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].matched_pattern, header.matched_pattern);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].header_name, header.header_name);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].value, header.value);
	ANIXOPS_EXPECT_STREQ(plan.header_rewrites[0].message, header.message);
	ANIXOPS_EXPECT_STREQ(header.header_name, "X-Current");
	ANIXOPS_EXPECT_STREQ(header.value, "resp-item");
	ANIXOPS_EXPECT_EQ_INT(plan.script.kind, ANIXOPS_SCRIPT_NONE);

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

static void loon_plugin_metadata_fixture_records_ignored_lines(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.PluginMetadata.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	anixops_mitm_decision_t mitm;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 1);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 6);

	for (i = 0; i < 5; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "plugin metadata ignored");
	}
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, 5, &diagnostic), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_ACCEPTED);
	ANIXOPS_EXPECT_STREQ(diagnostic.section, "MITM");
	ANIXOPS_EXPECT_STREQ(diagnostic.action, "hostname");

	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "plugin.metadata.loon.test", 0, &mitm), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(mitm.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
	free(fixture);
}

static void loon_plugin_metadata_unsupported_keys_are_not_claimed(void)
{
	char *fixture = read_fixture("tests/fixtures/Loon.PluginMetadata.Unsupported.plugin");
	anixops_engine_t *engine = anixops_engine_new();
	anixops_rule_diagnostic_t diagnostic;
	size_t i;
	ANIXOPS_EXPECT_TRUE(fixture != NULL);
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_set_compat_profile(engine, ANIXOPS_COMPAT_LOON_STRICT), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_engine_load_config(engine, fixture), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_argument_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rewrite_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_script_rule_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 0);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_rule_diagnostic_count(engine), 4);

	for (i = 0; i < 4; i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.status, ANIXOPS_RULE_DIAGNOSTIC_IGNORED);
		ANIXOPS_EXPECT_EQ_INT(diagnostic.profile, ANIXOPS_COMPAT_LOON_STRICT);
		ANIXOPS_EXPECT_STREQ(diagnostic.section, "Plugin");
		ANIXOPS_EXPECT_STREQ(diagnostic.action, "line");
		ANIXOPS_EXPECT_STREQ(diagnostic.message, "plugin metadata ignored");
	}

	anixops_engine_free(engine);
	free(fixture);
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
		"config/loon_mitm_options_fixture_exposes_adapter_flags",
		loon_mitm_options_fixture_exposes_adapter_flags);
	add_test(
		tests,
		count,
		cap,
		"config/loon_mitm_options_malformed_fixture_rejects_invalid_host",
		loon_mitm_options_malformed_fixture_rejects_invalid_host);
	add_test(
		tests,
		count,
		cap,
		"config/loon_mitm_certificate_unsupported_fixture_keeps_material_ignored",
		loon_mitm_certificate_unsupported_fixture_keeps_material_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_reject_fixture_maps_domain_suffix_rejects",
		loon_rule_domain_reject_fixture_maps_domain_suffix_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain",
		loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_exact_reject_fixture_maps_exact_domain_rejects",
		loon_rule_domain_exact_reject_fixture_maps_exact_domain_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain",
		loon_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects",
		loon_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword",
		loon_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_final_reject_fixture_maps_final_reject",
		loon_rule_final_reject_fixture_maps_final_reject);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_final_reject_malformed_fixture_rejects_missing_action",
		loon_rule_final_reject_malformed_fixture_rejects_missing_action);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_url_regex_reject_fixture_maps_url_regex_rejects",
		loon_rule_url_regex_reject_fixture_maps_url_regex_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_url_regex_reject_malformed_fixture_rejects_invalid_regex",
		loon_rule_url_regex_reject_malformed_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/loon_rule_route_unsupported_fixture_keeps_routes_ignored",
		loon_rule_route_unsupported_fixture_keeps_routes_ignored);
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
		"config/loon_argument_section_fixture_resolves_script_defaults",
		loon_argument_section_fixture_resolves_script_defaults);
	add_test(
		tests,
		count,
		cap,
		"config/loon_argument_section_malformed_fixture_rejects_missing_equals",
		loon_argument_section_malformed_fixture_rejects_missing_equals);
	add_test(
		tests,
		count,
		cap,
		"config/loon_script_metadata_fixture_exposes_dispatch_fields",
		loon_script_metadata_fixture_exposes_dispatch_fields);
	add_test(
		tests,
		count,
		cap,
		"config/loon_script_metadata_malformed_fixture_rejects_missing_path",
		loon_script_metadata_malformed_fixture_rejects_missing_path);
	add_test(
		tests,
		count,
		cap,
		"config/loon_task_metadata_fixture_emits_task_descriptors",
		loon_task_metadata_fixture_emits_task_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/loon_task_metadata_malformed_fixture_rejects_invalid_cron",
		loon_task_metadata_malformed_fixture_rejects_invalid_cron);
	add_test(
		tests,
		count,
		cap,
		"config/loon_task_unsupported_fixture_keeps_non_task_types_ignored",
		loon_task_unsupported_fixture_keeps_non_task_types_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/loon_inline_arguments_fixture_resolves_script_defaults",
		loon_inline_arguments_fixture_resolves_script_defaults);
	add_test(
		tests,
		count,
		cap,
		"config/loon_inline_arguments_malformed_fixture_rejects_missing_separator",
		loon_inline_arguments_malformed_fixture_rejects_missing_separator);
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
		"config/quantumultx_echo_response_fixture_maps_response_body",
		quantumultx_echo_response_fixture_maps_response_body);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_echo_response_malformed_fixture_rejects_missing_body",
		quantumultx_echo_response_malformed_fixture_rejects_missing_body);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_task_metadata_fixture_emits_task_descriptors",
		quantumultx_task_metadata_fixture_emits_task_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path",
		quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_task_metadata_unsupported_event_fixture_stays_ignored",
		quantumultx_task_metadata_unsupported_event_fixture_stays_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron",
		quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_mitm_options_fixture_exposes_adapter_flags",
		quantumultx_mitm_options_fixture_exposes_adapter_flags);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_mitm_options_malformed_fixture_rejects_invalid_host",
		quantumultx_mitm_options_malformed_fixture_rejects_invalid_host);
	add_test(
		tests,
		count,
		cap,
		"config/quantumultx_mitm_certificate_unsupported_fixture_keeps_options_ignored",
		quantumultx_mitm_certificate_unsupported_fixture_keeps_options_ignored);
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
		"config/surge_mitm_certificate_unsupported_fixture_keeps_material_ignored",
		surge_mitm_certificate_unsupported_fixture_keeps_material_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/surge_script_unsupported_fixture_keeps_non_task_types_ignored",
		surge_script_unsupported_fixture_keeps_non_task_types_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/surge_task_metadata_fixture_emits_task_descriptors",
		surge_task_metadata_fixture_emits_task_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/surge_task_metadata_malformed_fixture_rejects_invalid_cron",
		surge_task_metadata_malformed_fixture_rejects_invalid_cron);
	add_test(
		tests,
		count,
		cap,
		"config/surge_task_event_fixture_emits_event_descriptors",
		surge_task_event_fixture_emits_event_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/surge_task_event_malformed_fixture_rejects_missing_event_name",
		surge_task_event_malformed_fixture_rejects_missing_event_name);
	add_test(
		tests,
		count,
		cap,
		"config/surge_task_event_unsupported_fixture_rejects_unknown_event_name",
		surge_task_event_unsupported_fixture_rejects_unknown_event_name);
	add_test(
		tests,
		count,
		cap,
		"config/surge_requirement_metadata_fixture_records_tolerated_keys",
		surge_requirement_metadata_fixture_records_tolerated_keys);
	add_test(
		tests,
		count,
		cap,
		"config/surge_requirement_metadata_unsupported_keys_are_not_claimed",
		surge_requirement_metadata_unsupported_keys_are_not_claimed);
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
		"config/cron_task_trigger_common_fixture_emits_task_descriptors",
		cron_task_trigger_common_fixture_emits_task_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/cron_task_trigger_unsupported_fixture_does_not_register_descriptors",
		cron_task_trigger_unsupported_fixture_does_not_register_descriptors);
	add_test(
		tests,
		count,
		cap,
		"config/cron_task_trigger_malformed_fixture_rejects_invalid_cron",
		cron_task_trigger_malformed_fixture_rejects_invalid_cron);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_mitm_fixture_exposes_host_patterns",
		stash_http_mitm_fixture_exposes_host_patterns);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_mitm_malformed_fixture_rejects_invalid_host",
		stash_http_mitm_malformed_fixture_rejects_invalid_host);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_mitm_port_specific_fixture_stays_unsupported",
		stash_http_mitm_port_specific_fixture_stays_unsupported);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_mitm_certificate_unsupported_fixture_keeps_material_ignored",
		stash_http_mitm_certificate_unsupported_fixture_keeps_material_ignored);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_force_http_engine_fixture_exposes_quic_signal",
		stash_http_force_http_engine_fixture_exposes_quic_signal);
	add_test(
		tests,
		count,
		cap,
		"config/stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool",
		stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool);
	add_test(
		tests,
		count,
		cap,
		"config/stash_url_rewrite_fixture_maps_reject_subset",
		stash_url_rewrite_fixture_maps_reject_subset);
	add_test(
		tests,
		count,
		cap,
		"config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex",
		stash_url_rewrite_malformed_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/stash_url_rewrite_redirect_fixture_maps_redirect_subset",
		stash_url_rewrite_redirect_fixture_maps_redirect_subset);
	add_test(
		tests,
		count,
		cap,
		"config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex",
		stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/stash_migration_guard_fixture_stays_parser_unsupported",
		stash_migration_guard_fixture_stays_parser_unsupported);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_migration_guard_fixture_stays_parser_unsupported",
		shadowrocket_migration_guard_fixture_stays_parser_unsupported);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects",
		shadowrocket_rule_reject_fixture_maps_url_regex_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex",
		shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects",
		shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain",
		shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects",
		shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain",
		shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects",
		shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword",
		shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_final_reject_fixture_maps_final_reject",
		shadowrocket_rule_final_reject_fixture_maps_final_reject);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action",
		shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_common_config_fixture_is_supported",
		shadowrocket_common_config_fixture_is_supported);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_common_config_fixture_rejects_invalid_regex",
		shadowrocket_common_config_fixture_rejects_invalid_regex);
	add_test(
		tests,
		count,
		cap,
		"config/shadowrocket_mitm_certificate_unsupported_fixture_keeps_material_ignored",
		shadowrocket_mitm_certificate_unsupported_fixture_keeps_material_ignored);
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
		"config/loon_plugin_metadata_fixture_records_ignored_lines",
		loon_plugin_metadata_fixture_records_ignored_lines);
	add_test(
		tests,
		count,
		cap,
		"config/loon_plugin_metadata_unsupported_keys_are_not_claimed",
		loon_plugin_metadata_unsupported_keys_are_not_claimed);
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
