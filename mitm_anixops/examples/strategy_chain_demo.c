#include "mitm_anixops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
	fprintf(stderr, "demo assertion failed: %s\n", message);
	exit(1);
}

static void expect_int(const char *label, int got, int want)
{
	if (got != want) {
		fprintf(stderr, "demo assertion failed: %s got=%d want=%d\n", label, got, want);
		exit(1);
	}
}

static void expect_string(const char *label, const char *got, const char *want)
{
	if (strcmp(got, want) != 0) {
		fprintf(stderr, "demo assertion failed: %s got=%s want=%s\n", label, got, want);
		exit(1);
	}
}

int main(void)
{
	static const char config[] =
		"[Argument]\n"
		"Mode = select,demo\n"
		"\n"
		"[MITM]\n"
		"enabled = true\n"
		"hostname = api.example.com, -deny.example.com\n"
		"disable-quic = true\n"
		"\n"
		"[Rewrite]\n"
		"^http://old\\.example/(.*) https://new.example/$1 302\n"
		"^https://api\\.example\\.com/request/(.*) header-add X-Demo path-$1\n"
		"^https://api\\.example\\.com/response response-body-replace-regex ads clean\n"
		"^https://api\\.example\\.com/response response-header-replace X-Mode demo\n"
		"\n"
		"[Script]\n"
		"http-response ^https://api\\.example\\.com/script requires-body=1, script-path=https://scripts.example/demo.js, tag=demo.response, argument=[{Mode}]\n";

	anixops_engine_t *engine = anixops_engine_new();
	anixops_mitm_decision_t mitm;
	anixops_rewrite_result_t rewrite;
	anixops_header_rewrite_result_t header;
	anixops_script_result_t script;
	char body[128];
	char diagnostic[ANIXOPS_MESSAGE_CAP];
	int status = 0;
	size_t line = 0;
	int rc;

	if (engine == NULL) {
		fail("engine allocation");
	}

	rc = anixops_engine_load_config(engine, config);
	if (rc != ANIXOPS_OK) {
		(void)anixops_engine_copy_last_error(engine, &status, &line, diagnostic, sizeof(diagnostic));
		fprintf(stderr, "config failed status=%d line=%zu message=%s\n", status, line, diagnostic);
		anixops_engine_free(engine);
		return 1;
	}
	anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);

	printf("version=%s\n", anixops_version());
	printf(
		"counts mitm=%zu rewrite=%zu script=%zu argument=%zu\n",
		anixops_engine_mitm_pattern_count(engine),
		anixops_engine_rewrite_rule_count(engine),
		anixops_engine_script_rule_count(engine),
		anixops_engine_argument_count(engine));

	expect_int("mitm tcp rc", anixops_mitm_evaluate(engine, "api.example.com", 0, &mitm), ANIXOPS_OK);
	expect_int("mitm tcp decision", mitm.decision, ANIXOPS_MITM_INTERCEPT);
	expect_string("mitm tcp pattern", mitm.matched_pattern, "api.example.com");
	printf("mitm tcp decision=%d reason=%d pattern=%s\n", mitm.decision, mitm.reason, mitm.matched_pattern);

	expect_int("mitm quic rc", anixops_mitm_evaluate(engine, "api.example.com", 1, &mitm), ANIXOPS_OK);
	expect_int("mitm quic decision", mitm.decision, ANIXOPS_MITM_REJECT_QUIC);
	printf("mitm quic decision=%d reason=%d pattern=%s\n", mitm.decision, mitm.reason, mitm.matched_pattern);

	expect_int("mitm deny rc", anixops_mitm_evaluate(engine, "deny.example.com", 0, &mitm), ANIXOPS_OK);
	expect_int("mitm deny decision", mitm.decision, ANIXOPS_MITM_BYPASS);
	expect_int("mitm deny reason", mitm.reason, ANIXOPS_MITM_REASON_DENY_HOST);
	printf("mitm deny decision=%d reason=%d pattern=%s\n", mitm.decision, mitm.reason, mitm.matched_pattern);

	expect_int(
		"url rewrite rc",
		anixops_rewrite_evaluate_url(engine, "http://old.example/path", ANIXOPS_PHASE_REQUEST, &rewrite),
		ANIXOPS_OK);
	expect_int("url rewrite action", rewrite.action, ANIXOPS_REWRITE_REDIRECT_302);
	expect_string("url rewrite value", rewrite.value, "https://new.example/path");
	printf("rewrite url action=%d status=%d value=%s\n", rewrite.action, rewrite.status_code, rewrite.value);

	expect_int(
		"header rewrite rc",
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.example.com/request/item",
			ANIXOPS_PHASE_REQUEST,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	expect_int("header rewrite action", header.action, ANIXOPS_REWRITE_HEADER_ADD);
	expect_string("header rewrite name", header.header_name, "X-Demo");
	expect_string("header rewrite value", header.value, "path-item");
	printf("rewrite request-header action=%d name=%s value=%s\n", header.action, header.header_name, header.value);

	expect_int(
		"body rewrite rc",
		anixops_rewrite_apply_body(
			engine,
			"https://api.example.com/response",
			ANIXOPS_PHASE_RESPONSE,
			"ads=1&ok=1",
			body,
			sizeof(body),
			&rewrite),
		ANIXOPS_OK);
	expect_int("body rewrite action", rewrite.action, ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX);
	expect_string("body rewrite value", body, "clean=1&ok=1");
	printf("rewrite response-body action=%d body=%s\n", rewrite.action, body);

	expect_int(
		"response header rewrite rc",
		anixops_rewrite_evaluate_header(
			engine,
			"https://api.example.com/response",
			ANIXOPS_PHASE_RESPONSE,
			0,
			NULL,
			&header),
		ANIXOPS_OK);
	expect_int("response header action", header.action, ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE);
	expect_string("response header name", header.header_name, "X-Mode");
	expect_string("response header value", header.value, "demo");
	printf("rewrite response-header action=%d name=%s value=%s\n", header.action, header.header_name, header.value);

	expect_int(
		"script rc",
		anixops_script_evaluate_url(engine, "https://api.example.com/script", ANIXOPS_PHASE_RESPONSE, &script),
		ANIXOPS_OK);
	expect_int("script kind", script.kind, ANIXOPS_SCRIPT_HTTP_RESPONSE);
	expect_int("script requires body", script.requires_body, 1);
	expect_string("script path", script.script_path, "https://scripts.example/demo.js");
	expect_string("script argument", script.argument, "Mode=demo");
	printf(
		"script kind=%d requires_body=%d path=%s tag=%s argument=%s\n",
		script.kind,
		script.requires_body,
		script.script_path,
		script.tag,
		script.argument);

	anixops_engine_free(engine);
	return 0;
}
