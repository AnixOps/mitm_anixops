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

static anixops_engine_t *trusted_mitm_engine(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	if (engine == NULL) {
		return NULL;
	}
	anixops_engine_set_mitm_enabled(engine, 1);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	return engine;
}

static void disabled_mitm_bypasses_before_matching(void)
{
	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_DISABLED);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "");

	anixops_engine_free(engine);
}

static void empty_host_is_rejected_after_enabled_check(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "   ", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_EMPTY_HOST);

	anixops_engine_free(engine);
}

static void certificate_state_matrix_blocks_untrusted_states(void)
{
	anixops_cert_state_t states[] = {
		ANIXOPS_CERT_UNKNOWN,
		ANIXOPS_CERT_NOT_INSTALLED,
		ANIXOPS_CERT_INSTALLED_UNTRUSTED,
		ANIXOPS_CERT_REVOKED
	};
	size_t i;

	for (i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		anixops_engine_t *engine = anixops_engine_new();
		anixops_mitm_decision_t decision;
		ANIXOPS_EXPECT_TRUE(engine != NULL);
		ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
		anixops_engine_set_mitm_enabled(engine, 1);
		anixops_engine_set_cert_state(engine, states[i]);
		ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 0, &decision), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
		ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED);
		anixops_engine_free(engine);
	}
}

static void deny_pattern_wins_over_allow_pattern(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com, -secure.example.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "secure.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_DENY_HOST);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "secure.example.com");

	anixops_engine_free(engine);
}

static void wildcard_matches_subdomains_without_base_domain(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "*.example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "deep.api.example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "*.example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "example.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_NO_HOST_MATCH);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "badexample.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_NO_HOST_MATCH);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "");

	anixops_engine_free(engine);
}

static void hostname_policy_normalizes_trailing_dot_port_and_deny_wildcard_boundary(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_mitm_hostname(
			engine,
			"example.com., blocked.example.com., *.example.com., -*.blocked.example.com."),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 4);

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, " EXAMPLE.COM.:443 ", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "API.EXAMPLE.COM.", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "*.example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "badexample.com.", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_NO_HOST_MATCH);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "blocked.example.com.", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "blocked.example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.blocked.example.com:443", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_DENY_HOST);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "*.blocked.example.com");

	anixops_engine_free(engine);
}

static void host_normalization_handles_case_port_and_ipv6_literal(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "api.example.com,[2001:db8::1]"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, " API.EXAMPLE.COM:443 ", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "api.example.com");

	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "[2001:db8::1]:443", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

static void no_host_match_bypasses(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.other.test", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_NO_HOST_MATCH);

	anixops_engine_free(engine);
}

static void module_patch_markers_are_ignored_in_mitm_hostname(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(
		anixops_engine_add_mitm_hostname(engine, "%APPEND% app.bilibili.com, %INSERT% app.biliapi.net"),
		ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_SIZE(anixops_engine_mitm_pattern_count(engine), 2);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "app.bilibili.com", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "app.bilibili.com");
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "app.biliapi.net", 0, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);
	ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "app.biliapi.net");

	anixops_engine_free(engine);
}

static void quic_match_returns_reject_decision_by_default(void)
{
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*.example.com"), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 1, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_REJECT_QUIC);
	ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_QUIC_DISABLED_FOR_MITM);

	anixops_engine_set_disable_quic_for_mitm(engine, 0);
	ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, "api.example.com", 1, &decision), ANIXOPS_OK);
	ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_INTERCEPT);

	anixops_engine_free(engine);
}

static void malformed_runtime_hosts_do_not_intercept_even_with_wildcard(void)
{
	const char *hosts[] = {
		"http://api.example.com",
		".example.com",
		"bad..example.com",
		":",
		"api.example.com/path"
	};
	anixops_engine_t *engine = trusted_mitm_engine();
	anixops_mitm_decision_t decision;
	size_t i;
	ANIXOPS_EXPECT_TRUE(engine != NULL);

	ANIXOPS_EXPECT_EQ_INT(anixops_engine_add_mitm_hostname(engine, "*"), ANIXOPS_OK);
	for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
		ANIXOPS_EXPECT_EQ_INT(anixops_mitm_evaluate(engine, hosts[i], 0, &decision), ANIXOPS_OK);
		ANIXOPS_EXPECT_EQ_INT(decision.decision, ANIXOPS_MITM_BYPASS);
		ANIXOPS_EXPECT_EQ_INT(decision.reason, ANIXOPS_MITM_REASON_NO_HOST_MATCH);
		ANIXOPS_EXPECT_STREQ(decision.matched_pattern, "");
		ANIXOPS_EXPECT_STREQ(decision.message, "invalid host");
	}

	anixops_engine_free(engine);
}

void anixops_register_mitm_tests(anixops_test_case_t *tests, size_t *count, size_t cap)
{
	add_test(tests, count, cap, "mitm/disabled_mitm_bypasses_before_matching", disabled_mitm_bypasses_before_matching);
	add_test(tests, count, cap, "mitm/empty_host_is_rejected_after_enabled_check", empty_host_is_rejected_after_enabled_check);
	add_test(tests, count, cap, "mitm/certificate_state_matrix_blocks_untrusted_states", certificate_state_matrix_blocks_untrusted_states);
	add_test(tests, count, cap, "mitm/deny_pattern_wins_over_allow_pattern", deny_pattern_wins_over_allow_pattern);
	add_test(
		tests,
		count,
		cap,
		"mitm/wildcard_matches_subdomains_without_base_domain",
		wildcard_matches_subdomains_without_base_domain);
	add_test(
		tests,
		count,
		cap,
		"mitm/hostname_policy_normalizes_trailing_dot_port_and_deny_wildcard_boundary",
		hostname_policy_normalizes_trailing_dot_port_and_deny_wildcard_boundary);
	add_test(tests, count, cap, "mitm/host_normalization_handles_case_port_and_ipv6_literal", host_normalization_handles_case_port_and_ipv6_literal);
	add_test(tests, count, cap, "mitm/no_host_match_bypasses", no_host_match_bypasses);
	add_test(
		tests,
		count,
		cap,
		"mitm/module_patch_markers_are_ignored_in_mitm_hostname",
		module_patch_markers_are_ignored_in_mitm_hostname);
	add_test(tests, count, cap, "mitm/quic_match_returns_reject_decision_by_default", quic_match_returns_reject_decision_by_default);
	add_test(
		tests,
		count,
		cap,
		"mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard",
		malformed_runtime_hosts_do_not_intercept_even_with_wildcard);
}
