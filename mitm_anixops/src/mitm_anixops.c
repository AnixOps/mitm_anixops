#define _POSIX_C_SOURCE 200809L

#include "mitm_anixops.h"

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct anixops_host_pattern {
	char *pattern;
	int deny;
} anixops_host_pattern_t;

typedef struct anixops_rewrite_rule {
	char *source;
	char *pattern;
	char *replacement;
	char *body_pattern;
	char *header_name;
	char *header_pattern;
	regex_t regex;
	regex_t body_regex;
	regex_t header_regex;
	int regex_ready;
	int body_regex_ready;
	int header_regex_ready;
	anixops_rewrite_action_t action;
	anixops_phase_t phase;
	int status_code;
} anixops_rewrite_rule_t;

typedef struct anixops_script_rule {
	char *source;
	char *pattern;
	char *script_path;
	char *tag;
	char *argument_template;
	regex_t regex;
	int regex_ready;
	anixops_script_kind_t kind;
	anixops_phase_t phase;
	int requires_body;
} anixops_script_rule_t;

typedef struct anixops_argument {
	char *name;
	char *default_value;
	char *value;
} anixops_argument_t;

struct anixops_engine {
	anixops_host_pattern_t *hosts;
	size_t host_len;
	size_t host_cap;

	anixops_rewrite_rule_t *rules;
	size_t rule_len;
	size_t rule_cap;

	anixops_script_rule_t *scripts;
	size_t script_len;
	size_t script_cap;

	anixops_argument_t *arguments;
	size_t argument_len;
	size_t argument_cap;

	int mitm_enabled;
	int h2_mitm_enabled;
	int disable_quic_for_mitm;
	int skip_server_cert_verify;
	anixops_cert_state_t cert_state;

	int last_status;
	size_t last_error_line;
	char last_error_message[ANIXOPS_MESSAGE_CAP];
};

static char *anixops_strdup(const char *value);
static char *anixops_strndup(const char *value, size_t len);
static void anixops_free_rewrite_rule(anixops_rewrite_rule_t *rule);
static void anixops_free_script_rule(anixops_script_rule_t *rule);
static void anixops_free_argument(anixops_argument_t *argument);
static int anixops_reserve_hosts(anixops_engine_t *engine, size_t want);
static int anixops_reserve_rules(anixops_engine_t *engine, size_t want);
static int anixops_reserve_scripts(anixops_engine_t *engine, size_t want);
static int anixops_reserve_arguments(anixops_engine_t *engine, size_t want);
static void anixops_trim_inplace(char *value);
static void anixops_lower_inplace(char *value);
static int anixops_parse_bool(const char *value);
static int anixops_starts_with_ci(const char *value, const char *prefix);
static int anixops_is_rewrite_section(const char *key);
static int anixops_next_token(const char **cursor, char *out, size_t out_cap);
static int anixops_next_csv_field(const char **cursor, char *out, size_t out_cap);
static int anixops_compile_regex(regex_t *regex, const char *pattern, int flags);
static const char *anixops_regex_pattern_after_inline_flags(const char *pattern, int *flags);
static int anixops_normalize_regex_pattern(const char *pattern, char **out_pattern);
static int anixops_regex_append_literal_bytes(
	char *out,
	size_t out_cap,
	size_t *pos,
	const char *bytes,
	size_t len,
	int in_class);
static int anixops_regex_char_is_escaped(const char *pattern, size_t index);
static int anixops_regex_closes_interval_quantifier(const char *pattern, size_t close_index);
static int anixops_parse_rewrite_action(const char *token, anixops_rewrite_action_t *action, int *status_code);
static int anixops_rewrite_action_redirects(anixops_rewrite_action_t action);
static int anixops_rewrite_action_replaces_body(anixops_rewrite_action_t action);
static int anixops_rewrite_action_replaces_body_regex(anixops_rewrite_action_t action);
static int anixops_rewrite_action_replaces_body_json(anixops_rewrite_action_t action);
static int anixops_rewrite_action_rewrites_header(anixops_rewrite_action_t action);
static int anixops_rewrite_action_replaces_header_regex(anixops_rewrite_action_t action);
static int anixops_parse_script_kind(const char *token, anixops_script_kind_t *kind, anixops_phase_t *phase);
static int anixops_script_kind_requires_body_token(const char *token);
static anixops_phase_t anixops_default_phase_for_action(anixops_rewrite_action_t action);
static int anixops_extract_attr(const char *attrs, const char *key, char *out, size_t out_cap);
static int anixops_split_attr_segment(const char *segment, char *key, size_t key_cap, char *value, size_t value_cap);
static void anixops_unquote_inplace(char *value);
static int anixops_argument_index(const anixops_engine_t *engine, const char *name);
static int anixops_engine_set_argument_default(anixops_engine_t *engine, const char *name, const char *value);
static int anixops_engine_add_inline_arguments(anixops_engine_t *engine, const char *line);
static const char *anixops_argument_value(const anixops_engine_t *engine, const char *name);
static int anixops_resolve_script_argument(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap);
static int anixops_resolve_argument_list(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap);
static int anixops_resolve_argument_template(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap);
static int anixops_expand_replacement(
	const char *input,
	const char *replacement,
	const regmatch_t *matches,
	size_t match_count,
	char *out,
	size_t out_cap);
static int anixops_apply_body_regex_replacement(
	const char *body,
	const anixops_rewrite_rule_t *rule,
	char *out,
	size_t out_cap,
	int *out_replaced);
static int anixops_apply_body_json_replacement(
	const char *body,
	const anixops_rewrite_rule_t *rule,
	char *out,
	size_t out_cap,
	int *out_replaced,
	int *out_path_supported);
static int anixops_apply_regex_replacement(
	const char *input,
	const regex_t *regex,
	int regex_ready,
	const char *replacement,
	char *out,
	size_t out_cap,
	int *out_replaced);
static int anixops_append_expanded_replacement(
	const char *input,
	const char *replacement,
	const regmatch_t *matches,
	size_t match_count,
	char *out,
	size_t out_cap,
	size_t *pos);
static int anixops_append_json_replacement(char *out, size_t out_cap, size_t *pos, const char *replacement);
static int anixops_append_json_string(char *out, size_t out_cap, size_t *pos, const char *value);
static int anixops_json_find_path_value(
	const char *body,
	const char *path,
	const char **out_value_start,
	const char **out_value_end);
static int anixops_json_find_member_value(
	const char *object_start,
	const char *object_end,
	const char *key,
	size_t key_len,
	const char **out_value_start,
	const char **out_value_end);
static int anixops_json_find_array_element_value(
	const char *array_start,
	const char *array_end,
	size_t index,
	const char **out_value_start,
	const char **out_value_end);
static int anixops_json_key_matches(const char *string_start, const char *string_end, const char *key, size_t key_len);
static int anixops_json_parse_path_segment(const char **cursor, const char **out_key, size_t *out_key_len);
static int anixops_json_parse_bracket_key(
	const char **cursor,
	char *out,
	size_t out_cap,
	const char **out_key,
	size_t *out_key_len);
static int anixops_json_decode_escape(const char **cursor, const char *end, char *out, size_t out_cap, size_t *out_len);
static int anixops_json_read_hex4(const char **cursor, const char *end, unsigned long *out_value);
static int anixops_json_append_utf8(char *out, size_t out_cap, size_t *pos, unsigned long codepoint);
static int anixops_hex_digit(unsigned char ch);
static int anixops_json_parse_array_index(const char **cursor, size_t *out_index);
static int anixops_json_replacement_is_raw_value(const char *value, const char **out_start, const char **out_end);
static const char *anixops_json_skip_ws(const char *cursor, const char *end);
static const char *anixops_json_skip_string(const char *cursor, const char *end);
static const char *anixops_json_skip_number(const char *cursor, const char *end);
static const char *anixops_json_skip_value(const char *cursor, const char *end);
static int anixops_append_range(char *out, size_t out_cap, size_t *pos, const char *start, size_t len);
static int anixops_append_char(char *out, size_t out_cap, size_t *pos, char value);
static int anixops_wildcard_match(const char *pattern, const char *text);
static void anixops_normalize_host(const char *input, char *out, size_t out_cap);
static int anixops_host_pattern_matches(const char *pattern, const char *host);
static void anixops_set_mitm_result(
	anixops_mitm_decision_t *out,
	anixops_mitm_decision_type_t decision,
	anixops_mitm_reason_t reason,
	const char *pattern,
	const char *message);
static void anixops_set_rewrite_none(anixops_rewrite_result_t *out);
static void anixops_set_header_rewrite_none(anixops_header_rewrite_result_t *out);
static void anixops_set_script_none(anixops_script_result_t *out);
static void anixops_set_diagnostic(anixops_engine_t *engine, int status, size_t line, const char *message);
static void anixops_clear_diagnostic(anixops_engine_t *engine);
static int anixops_config_line_error(anixops_engine_t *engine, int status, size_t line_no, char *line);
static void anixops_copy_text(char *dst, size_t cap, const char *src);
static int anixops_copy_text_checked(char *dst, size_t cap, const char *src);

ANIXOPS_API const char *anixops_version(void)
{
	return "0.24.0";
}

ANIXOPS_API const char *anixops_status_message(int status)
{
	switch (status) {
	case ANIXOPS_OK:
		return "ok";
	case ANIXOPS_ERR_INVALID_ARGUMENT:
		return "invalid argument";
	case ANIXOPS_ERR_OUT_OF_MEMORY:
		return "out of memory";
	case ANIXOPS_ERR_REGEX:
		return "regex compile failed";
	case ANIXOPS_ERR_CAPACITY:
		return "output capacity exceeded";
	case ANIXOPS_ERR_PARSE:
		return "parse failed";
	default:
		return "unknown status";
	}
}

static int anixops_is_rewrite_section(const char *key)
{
	return key != NULL &&
		(strcasecmp(key, "rewrite") == 0 ||
			strcasecmp(key, "rewrite_local") == 0 ||
			strcasecmp(key, "rewrite local") == 0 ||
			strcasecmp(key, "url rewrite") == 0 ||
			strcasecmp(key, "remote rewrite") == 0 ||
			strcasecmp(key, "header rewrite") == 0 ||
			strcasecmp(key, "remote header rewrite") == 0 ||
			strcasecmp(key, "header_rewrite") == 0 ||
			strcasecmp(key, "remote_header_rewrite") == 0);
}

ANIXOPS_API anixops_engine_t *anixops_engine_new(void)
{
	anixops_engine_t *engine = (anixops_engine_t *)calloc(1, sizeof(*engine));
	if (engine == NULL) {
		return NULL;
	}
	engine->mitm_enabled = 0;
	engine->h2_mitm_enabled = 1;
	engine->disable_quic_for_mitm = 1;
	engine->skip_server_cert_verify = 0;
	engine->cert_state = ANIXOPS_CERT_UNKNOWN;
	anixops_clear_diagnostic(engine);
	return engine;
}

ANIXOPS_API void anixops_engine_free(anixops_engine_t *engine)
{
	if (engine == NULL) {
		return;
	}
	anixops_engine_clear(engine);
	free(engine);
}

ANIXOPS_API void anixops_engine_clear(anixops_engine_t *engine)
{
	size_t i;
	if (engine == NULL) {
		return;
	}
	for (i = 0; i < engine->host_len; i++) {
		free(engine->hosts[i].pattern);
	}
	free(engine->hosts);
	engine->hosts = NULL;
	engine->host_len = 0;
	engine->host_cap = 0;

	for (i = 0; i < engine->rule_len; i++) {
		anixops_free_rewrite_rule(&engine->rules[i]);
	}
	free(engine->rules);
	engine->rules = NULL;
	engine->rule_len = 0;
	engine->rule_cap = 0;

	for (i = 0; i < engine->script_len; i++) {
		anixops_free_script_rule(&engine->scripts[i]);
	}
	free(engine->scripts);
	engine->scripts = NULL;
	engine->script_len = 0;
	engine->script_cap = 0;

	for (i = 0; i < engine->argument_len; i++) {
		anixops_free_argument(&engine->arguments[i]);
	}
	free(engine->arguments);
	engine->arguments = NULL;
	engine->argument_len = 0;
	engine->argument_cap = 0;

	engine->mitm_enabled = 0;
	engine->h2_mitm_enabled = 1;
	engine->disable_quic_for_mitm = 1;
	engine->skip_server_cert_verify = 0;
	engine->cert_state = ANIXOPS_CERT_UNKNOWN;
	anixops_clear_diagnostic(engine);
}

ANIXOPS_API int anixops_engine_copy_last_error(
	const anixops_engine_t *engine,
	int *out_status,
	size_t *out_line,
	char *out_message,
	size_t out_message_cap)
{
	int rc = ANIXOPS_OK;
	if (engine == NULL || (out_message == NULL && out_message_cap != 0) ||
		(out_message != NULL && out_message_cap == 0)) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	if (out_status != NULL) {
		*out_status = engine->last_status;
	}
	if (out_line != NULL) {
		*out_line = engine->last_error_line;
	}
	if (out_message != NULL) {
		rc = anixops_copy_text_checked(out_message, out_message_cap, engine->last_error_message);
	}
	return rc;
}

ANIXOPS_API int anixops_engine_load_config(anixops_engine_t *engine, const char *config_text)
{
	const char *cursor;
	size_t line_no = 1;
	enum {
		SECTION_NONE,
		SECTION_ARGUMENT,
		SECTION_REWRITE,
		SECTION_SCRIPT,
		SECTION_MITM
	} section = SECTION_NONE;

	if (engine == NULL || config_text == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "config text is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}

	anixops_clear_diagnostic(engine);
	cursor = config_text;

	while (*cursor != '\0') {
		const char *line_start = cursor;
		const char *line_end = strchr(cursor, '\n');
		size_t line_len = line_end == NULL ? strlen(cursor) : (size_t)(line_end - line_start);
		char *line = anixops_strndup(line_start, line_len);
		char *eq;
		char *key;
		char *value;
		size_t len;

		if (line == NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, line_no, "out of memory copying config line");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		if (line[0] != '\0' && line[strlen(line) - 1] == '\r') {
			line[strlen(line) - 1] = '\0';
		}
		anixops_trim_inplace(line);
		len = strlen(line);
		if (len >= 3 && line[0] == '#' && line[1] == '[' && line[len - 1] == ']') {
			line[len - 1] = '\0';
			key = line + 2;
			anixops_trim_inplace(key);
			if (anixops_is_rewrite_section(key)) {
				section = SECTION_REWRITE;
			}
			else if (strcasecmp(key, "mitm") == 0) {
				section = SECTION_MITM;
			}
			else if (strcasecmp(key, "script") == 0 || strcasecmp(key, "remote script") == 0) {
				section = SECTION_SCRIPT;
			}
			else if (strcasecmp(key, "argument") == 0 || strcasecmp(key, "arguments") == 0) {
				section = SECTION_ARGUMENT;
			}
			else {
				section = SECTION_NONE;
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (anixops_starts_with_ci(line, "#!arguments") &&
			(line[11] == '\0' || line[11] == '=' || isspace((unsigned char)line[11]))) {
			eq = strchr(line, '=');
			if (eq != NULL) {
				int rc;
				value = eq + 1;
				anixops_trim_inplace(value);
				rc = anixops_engine_add_inline_arguments(engine, value);
				if (rc != ANIXOPS_OK) {
					return anixops_config_line_error(engine, rc, line_no, line);
				}
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (line[0] == '\0' || line[0] == '#' || (line[0] == '/' && line[1] == '/')) {
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (len >= 2 && line[0] == '[' && line[len - 1] == ']') {
			line[len - 1] = '\0';
			key = line + 1;
			anixops_trim_inplace(key);
			if (strcasecmp(key, "argument") == 0 || strcasecmp(key, "arguments") == 0) {
				section = SECTION_ARGUMENT;
			}
			else if (anixops_is_rewrite_section(key)) {
				section = SECTION_REWRITE;
			}
			else if (strcasecmp(key, "mitm") == 0) {
				section = SECTION_MITM;
			}
			else if (strcasecmp(key, "script") == 0 || strcasecmp(key, "remote script") == 0) {
				section = SECTION_SCRIPT;
			}
			else {
				section = SECTION_NONE;
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}

		if (section == SECTION_REWRITE) {
			size_t script_count = engine->script_len;
			int rc = anixops_engine_add_script_rule(engine, line);
			if (rc != ANIXOPS_OK) {
				return anixops_config_line_error(engine, rc, line_no, line);
			}
			if (engine->script_len != script_count) {
				free(line);
				if (line_end == NULL) {
					break;
				}
				cursor = line_end + 1;
				line_no++;
				continue;
			}
			rc = anixops_engine_add_rewrite_rule(engine, line);
			if (rc != ANIXOPS_OK) {
				return anixops_config_line_error(engine, rc, line_no, line);
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (section == SECTION_SCRIPT) {
			int rc = anixops_engine_add_script_rule(engine, line);
			if (rc != ANIXOPS_OK) {
				return anixops_config_line_error(engine, rc, line_no, line);
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (section == SECTION_ARGUMENT) {
			int rc = anixops_engine_add_argument(engine, line);
			if (rc != ANIXOPS_OK) {
				return anixops_config_line_error(engine, rc, line_no, line);
			}
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		if (section != SECTION_MITM) {
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}

		eq = strchr(line, '=');
		if (eq == NULL) {
			free(line);
			if (line_end == NULL) {
				break;
			}
			cursor = line_end + 1;
			line_no++;
			continue;
		}
		*eq = '\0';
		key = line;
		value = eq + 1;
		anixops_trim_inplace(key);
		anixops_trim_inplace(value);
		if (strcasecmp(key, "hostname") == 0) {
			int rc = anixops_engine_add_mitm_hostname(engine, value);
			if (rc != ANIXOPS_OK) {
				return anixops_config_line_error(engine, rc, line_no, line);
			}
		}
		else if (strcasecmp(key, "skip-server-cert-verify") == 0) {
			engine->skip_server_cert_verify = anixops_parse_bool(value);
		}
		else if (strcasecmp(key, "enable") == 0 || strcasecmp(key, "enabled") == 0) {
			engine->mitm_enabled = anixops_parse_bool(value);
		}
		else if (strcasecmp(key, "h2") == 0 || strcasecmp(key, "h2-enable") == 0) {
			engine->h2_mitm_enabled = anixops_parse_bool(value);
		}
		else if (strcasecmp(key, "disable-quic") == 0 || strcasecmp(key, "disable-mitm-quic") == 0) {
			engine->disable_quic_for_mitm = anixops_parse_bool(value);
		}
		free(line);
		if (line_end == NULL) {
			break;
		}
		cursor = line_end + 1;
		line_no++;
	}

	anixops_clear_diagnostic(engine);
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_engine_add_rewrite_rule(anixops_engine_t *engine, const char *line)
{
	const char *cursor;
	char pattern[512];
	char second[2048];
	char third[2048];
	char fourth[2048];
	char fifth[2048];
	char sixth[2048];
	int have_third;
	int have_fourth;
	int have_fifth;
	int have_sixth;
	anixops_rewrite_action_t action = ANIXOPS_REWRITE_NONE;
	int status_code = 0;
	const char *body_pattern = "";
	const char *header_name = "";
	const char *header_pattern = "";
	const char *replacement = "";
	anixops_rewrite_rule_t *rule;

	if (engine == NULL || line == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "rewrite rule line is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_clear_diagnostic(engine);

	cursor = line;
	if (!anixops_next_token(&cursor, pattern, sizeof(pattern))) {
		return ANIXOPS_OK;
	}
	if (pattern[0] == '#' || (pattern[0] == '/' && pattern[1] == '/')) {
		return ANIXOPS_OK;
	}
	if (!anixops_next_token(&cursor, second, sizeof(second))) {
		return ANIXOPS_OK;
	}
	have_third = anixops_next_token(&cursor, third, sizeof(third));
	have_fourth = anixops_next_token(&cursor, fourth, sizeof(fourth));
	have_fifth = anixops_next_token(&cursor, fifth, sizeof(fifth));
	have_sixth = anixops_next_token(&cursor, sixth, sizeof(sixth));

	if (strcasecmp(second, "url") == 0 && have_third && anixops_parse_rewrite_action(third, &action, &status_code)) {
		if (anixops_rewrite_action_replaces_body(action)) {
			if (!have_fourth) {
				return ANIXOPS_OK;
			}
			body_pattern = fourth;
			replacement = have_fifth ? fifth : "";
		}
		else if (anixops_rewrite_action_rewrites_header(action)) {
			if (!have_fourth) {
				return ANIXOPS_OK;
			}
			header_name = fourth;
			if (anixops_rewrite_action_replaces_header_regex(action)) {
				if (!have_fifth) {
					return ANIXOPS_OK;
				}
				header_pattern = fifth;
				replacement = have_sixth ? sixth : "";
			}
			else {
				replacement = have_fifth ? fifth : "";
			}
		}
		else {
			replacement = have_fourth ? fourth : "";
		}
	}
	else if (anixops_parse_rewrite_action(second, &action, &status_code)) {
		if (anixops_rewrite_action_replaces_body(action)) {
			if (!have_third) {
				return ANIXOPS_OK;
			}
			body_pattern = third;
			replacement = have_fourth ? fourth : "";
		}
		else if (anixops_rewrite_action_rewrites_header(action)) {
			if (!have_third) {
				return ANIXOPS_OK;
			}
			header_name = third;
			if (anixops_rewrite_action_replaces_header_regex(action)) {
				if (!have_fourth) {
					return ANIXOPS_OK;
				}
				header_pattern = fourth;
				replacement = have_fifth ? fifth : "";
			}
			else {
				replacement = have_fourth ? fourth : "";
			}
		}
		else {
			replacement = have_third ? third : "";
		}
	}
	else if (have_third && anixops_parse_rewrite_action(third, &action, &status_code)) {
		if (anixops_rewrite_action_replaces_body(action)) {
			body_pattern = second;
			replacement = have_fourth ? fourth : "";
		}
		else if (anixops_rewrite_action_rewrites_header(action)) {
			header_name = second;
			if (anixops_rewrite_action_replaces_header_regex(action)) {
				if (!have_fourth) {
					return ANIXOPS_OK;
				}
				header_pattern = fourth;
				replacement = have_fifth ? fifth : "";
			}
			else {
				replacement = have_fourth ? fourth : "";
			}
		}
		else {
			replacement = second;
		}
	}
	else {
		return ANIXOPS_OK;
	}
	if (anixops_rewrite_action_replaces_body(action) && body_pattern[0] == '\0') {
		return ANIXOPS_OK;
	}
	if (anixops_rewrite_action_rewrites_header(action) && header_name[0] == '\0') {
		return ANIXOPS_OK;
	}
	if (anixops_rewrite_action_replaces_header_regex(action) && header_pattern[0] == '\0') {
		return ANIXOPS_OK;
	}

	if (anixops_reserve_rules(engine, engine->rule_len + 1) != ANIXOPS_OK) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory reserving rewrite rule");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	rule = &engine->rules[engine->rule_len];
	memset(rule, 0, sizeof(*rule));
	rule->source = anixops_strdup(line);
	rule->pattern = anixops_strdup(pattern);
	rule->replacement = anixops_strdup(replacement == NULL ? "" : replacement);
	rule->body_pattern = anixops_strdup(body_pattern == NULL ? "" : body_pattern);
	rule->header_name = anixops_strdup(header_name == NULL ? "" : header_name);
	rule->header_pattern = anixops_strdup(header_pattern == NULL ? "" : header_pattern);
	if (rule->source == NULL || rule->pattern == NULL || rule->replacement == NULL || rule->body_pattern == NULL ||
		rule->header_name == NULL || rule->header_pattern == NULL) {
		anixops_free_rewrite_rule(rule);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying rewrite rule");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	if (anixops_compile_regex(&rule->regex, rule->pattern, REG_EXTENDED) != 0) {
		anixops_free_rewrite_rule(rule);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_REGEX, 0, "rewrite URL regex failed to compile");
		return ANIXOPS_ERR_REGEX;
	}
	rule->regex_ready = 1;
	if (anixops_rewrite_action_replaces_body_regex(action)) {
		if (anixops_compile_regex(&rule->body_regex, rule->body_pattern, REG_EXTENDED) != 0) {
			anixops_free_rewrite_rule(rule);
			anixops_set_diagnostic(engine, ANIXOPS_ERR_REGEX, 0, "rewrite body regex failed to compile");
			return ANIXOPS_ERR_REGEX;
		}
		rule->body_regex_ready = 1;
	}
	if (anixops_rewrite_action_replaces_header_regex(action)) {
		if (anixops_compile_regex(&rule->header_regex, rule->header_pattern, REG_EXTENDED) != 0) {
			anixops_free_rewrite_rule(rule);
			anixops_set_diagnostic(engine, ANIXOPS_ERR_REGEX, 0, "rewrite header regex failed to compile");
			return ANIXOPS_ERR_REGEX;
		}
		rule->header_regex_ready = 1;
	}
	rule->action = action;
	rule->phase = anixops_default_phase_for_action(action);
	rule->status_code = status_code;
	engine->rule_len++;
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_engine_add_argument(anixops_engine_t *engine, const char *line)
{
	char *copy;
	char *eq;
	char *name;
	char *definition;
	const char *cursor;
	char kind[128];
	char default_value[2048];
	int rc;

	if (engine == NULL || line == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "argument line is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_clear_diagnostic(engine);

	copy = anixops_strdup(line);
	if (copy == NULL) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying argument line");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	anixops_trim_inplace(copy);
	if (copy[0] == '\0' || copy[0] == '#' || (copy[0] == '/' && copy[1] == '/')) {
		free(copy);
		return ANIXOPS_OK;
	}
	eq = strchr(copy, '=');
	if (eq == NULL) {
		free(copy);
		return ANIXOPS_OK;
	}
	*eq = '\0';
	name = copy;
	definition = eq + 1;
	anixops_trim_inplace(name);
	anixops_trim_inplace(definition);
	if (name[0] == '\0') {
		free(copy);
		return ANIXOPS_OK;
	}

	cursor = definition;
	if (!anixops_next_csv_field(&cursor, kind, sizeof(kind))) {
		free(copy);
		return ANIXOPS_OK;
	}
	default_value[0] = '\0';
	(void)anixops_next_csv_field(&cursor, default_value, sizeof(default_value));
	anixops_unquote_inplace(default_value);

	rc = anixops_engine_set_argument_default(engine, name, default_value);
	free(copy);
	if (rc != ANIXOPS_OK && engine->last_status == ANIXOPS_OK) {
		anixops_set_diagnostic(engine, rc, 0, anixops_status_message(rc));
	}
	return rc;
}

ANIXOPS_API int anixops_engine_set_argument_value(anixops_engine_t *engine, const char *name, const char *value)
{
	int idx;
	anixops_argument_t *slot;
	char *next_value;

	if (engine == NULL || name == NULL || value == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "argument name or value is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_clear_diagnostic(engine);
	if (name[0] == '\0') {
		return ANIXOPS_OK;
	}
	idx = anixops_argument_index(engine, name);
	if (idx < 0) {
		if (anixops_reserve_arguments(engine, engine->argument_len + 1) != ANIXOPS_OK) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory reserving argument");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		slot = &engine->arguments[engine->argument_len];
		memset(slot, 0, sizeof(*slot));
		slot->name = anixops_strdup(name);
		slot->default_value = anixops_strdup("");
		if (slot->name == NULL || slot->default_value == NULL) {
			anixops_free_argument(slot);
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying argument");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		idx = (int)engine->argument_len;
		engine->argument_len++;
	}
	next_value = anixops_strdup(value);
	if (next_value == NULL) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying argument value");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	free(engine->arguments[idx].value);
	engine->arguments[idx].value = next_value;
	return ANIXOPS_OK;
}

static int anixops_engine_set_argument_default(anixops_engine_t *engine, const char *name, const char *value)
{
	int idx;
	anixops_argument_t *slot;

	if (engine == NULL || name == NULL || value == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "argument default name or value is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	if (name[0] == '\0') {
		return ANIXOPS_OK;
	}
	idx = anixops_argument_index(engine, name);
	if (idx >= 0) {
		char *next_default = anixops_strdup(value);
		if (next_default == NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying argument default");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		free(engine->arguments[idx].default_value);
		engine->arguments[idx].default_value = next_default;
		return ANIXOPS_OK;
	}

	if (anixops_reserve_arguments(engine, engine->argument_len + 1) != ANIXOPS_OK) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory reserving argument");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	slot = &engine->arguments[engine->argument_len];
	memset(slot, 0, sizeof(*slot));
	slot->name = anixops_strdup(name);
	slot->default_value = anixops_strdup(value);
	if (slot->name == NULL || slot->default_value == NULL) {
		anixops_free_argument(slot);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying argument default");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	engine->argument_len++;
	return ANIXOPS_OK;
}

static int anixops_engine_add_inline_arguments(anixops_engine_t *engine, const char *line)
{
	char *copy;
	const char *cursor;
	char field[4096];

	if (engine == NULL || line == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "inline arguments line is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	copy = anixops_strdup(line);
	if (copy == NULL) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying inline arguments");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	cursor = copy;
	while (anixops_next_csv_field(&cursor, field, sizeof(field))) {
		char *colon;
		char *name;
		char *value;
		int rc;
		if (field[0] == '\0') {
			continue;
		}
		colon = strchr(field, ':');
		if (colon == NULL) {
			continue;
		}
		*colon = '\0';
		name = field;
		value = colon + 1;
		anixops_trim_inplace(name);
		anixops_trim_inplace(value);
		anixops_unquote_inplace(value);
		rc = anixops_engine_set_argument_default(engine, name, value);
		if (rc != ANIXOPS_OK) {
			free(copy);
			return rc;
		}
	}
	free(copy);
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_engine_add_script_rule(anixops_engine_t *engine, const char *line)
{
	char *copy;
	char *trimmed;
	const char *cursor;
	char first[128];
	char pattern[512];
	char type[128];
	char script_path[2048];
	char tag[2048];
	char argument_template[4096];
	char requires_body[128];
	anixops_script_kind_t kind = ANIXOPS_SCRIPT_NONE;
	anixops_phase_t phase = ANIXOPS_PHASE_REQUEST;
	const char *attrs = NULL;
	int direct_script_path = 0;
	anixops_script_rule_t *rule;

	if (engine == NULL || line == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "script rule line is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_clear_diagnostic(engine);

	copy = anixops_strdup(line);
	if (copy == NULL) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying script rule");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	trimmed = copy;
	anixops_trim_inplace(trimmed);
	if (trimmed[0] == '\0' || trimmed[0] == '#' || (trimmed[0] == '/' && trimmed[1] == '/')) {
		free(copy);
		return ANIXOPS_OK;
	}

	pattern[0] = '\0';
	type[0] = '\0';
	script_path[0] = '\0';
	tag[0] = '\0';
	argument_template[0] = '\0';
	requires_body[0] = '\0';

	cursor = trimmed;
	if (anixops_next_token(&cursor, first, sizeof(first)) && anixops_parse_script_kind(first, &kind, &phase)) {
		if (!anixops_next_token(&cursor, pattern, sizeof(pattern))) {
			free(copy);
			return ANIXOPS_OK;
		}
		attrs = cursor;
	}
	else {
		char *eq = strchr(trimmed, '=');
		char *left;
		if (eq == NULL) {
			const char *rewrite_cursor = trimmed;
			char mode[128];
			char rewrite_type[128];
			if (!anixops_next_token(&rewrite_cursor, pattern, sizeof(pattern)) ||
				!anixops_next_token(&rewrite_cursor, mode, sizeof(mode))) {
				free(copy);
				return ANIXOPS_OK;
			}
			if (anixops_parse_script_kind(mode, &kind, &phase)) {
				anixops_copy_text(rewrite_type, sizeof(rewrite_type), mode);
				if (!anixops_next_token(&rewrite_cursor, script_path, sizeof(script_path))) {
					free(copy);
					return ANIXOPS_OK;
				}
			}
			else if ((strcasecmp(mode, "url") == 0 || strcasecmp(mode, "header") == 0) &&
				anixops_next_token(&rewrite_cursor, rewrite_type, sizeof(rewrite_type)) &&
				anixops_parse_script_kind(rewrite_type, &kind, &phase)) {
				if (!anixops_next_token(&rewrite_cursor, script_path, sizeof(script_path))) {
					free(copy);
					return ANIXOPS_OK;
				}
			}
			else {
				free(copy);
				return ANIXOPS_OK;
			}
			if (anixops_script_kind_requires_body_token(rewrite_type)) {
				anixops_copy_text(requires_body, sizeof(requires_body), "1");
			}
			direct_script_path = 1;
			attrs = "";
		}
		else {
			*eq = '\0';
			left = trimmed;
			attrs = eq + 1;
			anixops_trim_inplace(left);
			if (left[0] == '\0') {
				free(copy);
				return ANIXOPS_OK;
			}
			anixops_copy_text(tag, sizeof(tag), left);
			if (!anixops_extract_attr(attrs, "type", type, sizeof(type)) ||
				!anixops_parse_script_kind(type, &kind, &phase)) {
				free(copy);
				return ANIXOPS_OK;
			}
			if (!anixops_extract_attr(attrs, "pattern", pattern, sizeof(pattern))) {
				free(copy);
				return ANIXOPS_OK;
			}
		}
	}

	if (!direct_script_path) {
		(void)anixops_extract_attr(attrs, "script-path", script_path, sizeof(script_path));
		if (script_path[0] == '\0') {
			(void)anixops_extract_attr(attrs, "script_path", script_path, sizeof(script_path));
		}
	}
	(void)anixops_extract_attr(attrs, "tag", tag, sizeof(tag));
	(void)anixops_extract_attr(attrs, "argument", argument_template, sizeof(argument_template));
	(void)anixops_extract_attr(attrs, "requires-body", requires_body, sizeof(requires_body));
	if (requires_body[0] == '\0') {
		(void)anixops_extract_attr(attrs, "requires_body", requires_body, sizeof(requires_body));
	}
	anixops_unquote_inplace(pattern);
	anixops_unquote_inplace(script_path);
	anixops_unquote_inplace(tag);
	anixops_unquote_inplace(argument_template);
	anixops_unquote_inplace(requires_body);

	if (kind == ANIXOPS_SCRIPT_NONE || pattern[0] == '\0' || script_path[0] == '\0') {
		free(copy);
		return ANIXOPS_OK;
	}
	if (anixops_reserve_scripts(engine, engine->script_len + 1) != ANIXOPS_OK) {
		free(copy);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory reserving script rule");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	rule = &engine->scripts[engine->script_len];
	memset(rule, 0, sizeof(*rule));
	rule->source = anixops_strdup(line);
	rule->pattern = anixops_strdup(pattern);
	rule->script_path = anixops_strdup(script_path);
	rule->tag = anixops_strdup(tag);
	rule->argument_template = anixops_strdup(argument_template);
	if (rule->source == NULL || rule->pattern == NULL || rule->script_path == NULL ||
		rule->tag == NULL || rule->argument_template == NULL) {
		anixops_free_script_rule(rule);
		free(copy);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying script rule");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	if (anixops_compile_regex(&rule->regex, rule->pattern, REG_EXTENDED) != 0) {
		anixops_free_script_rule(rule);
		free(copy);
		anixops_set_diagnostic(engine, ANIXOPS_ERR_REGEX, 0, "script URL regex failed to compile");
		return ANIXOPS_ERR_REGEX;
	}
	rule->regex_ready = 1;
	rule->kind = kind;
	rule->phase = phase;
	rule->requires_body = anixops_parse_bool(requires_body);
	engine->script_len++;
	free(copy);
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_engine_add_mitm_hostname(anixops_engine_t *engine, const char *pattern)
{
	char *copy;
	char *saveptr;
	char *item;

	if (engine == NULL || pattern == NULL) {
		if (engine != NULL) {
			anixops_set_diagnostic(engine, ANIXOPS_ERR_INVALID_ARGUMENT, 0, "mitm hostname pattern is null");
		}
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_clear_diagnostic(engine);

	copy = anixops_strdup(pattern);
	if (copy == NULL) {
		anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying mitm hostname pattern");
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}

	for (item = strtok_r(copy, ",", &saveptr); item != NULL; item = strtok_r(NULL, ",", &saveptr)) {
		anixops_host_pattern_t *slot;
		int deny = 0;
		anixops_trim_inplace(item);
		if (item[0] == '\0') {
			continue;
		}
		if (anixops_starts_with_ci(item, "%APPEND%")) {
			item += strlen("%APPEND%");
			anixops_trim_inplace(item);
		}
		if (item[0] == '-' || item[0] == '!') {
			deny = 1;
			item++;
			anixops_trim_inplace(item);
		}
		else if (item[0] == '+') {
			item++;
			anixops_trim_inplace(item);
		}
		if (item[0] == '\0') {
			continue;
		}
		if (anixops_reserve_hosts(engine, engine->host_len + 1) != ANIXOPS_OK) {
			free(copy);
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory reserving mitm hostname");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		slot = &engine->hosts[engine->host_len];
		{
			char normalized[512];
			anixops_normalize_host(item, normalized, sizeof(normalized));
			slot->pattern = anixops_strdup(normalized);
		}
		if (slot->pattern == NULL) {
			free(copy);
			anixops_set_diagnostic(engine, ANIXOPS_ERR_OUT_OF_MEMORY, 0, "out of memory copying mitm hostname");
			return ANIXOPS_ERR_OUT_OF_MEMORY;
		}
		slot->deny = deny;
		engine->host_len++;
	}

	free(copy);
	return ANIXOPS_OK;
}

ANIXOPS_API void anixops_engine_set_mitm_enabled(anixops_engine_t *engine, int enabled)
{
	if (engine != NULL) {
		engine->mitm_enabled = enabled ? 1 : 0;
	}
}

ANIXOPS_API void anixops_engine_set_h2_mitm_enabled(anixops_engine_t *engine, int enabled)
{
	if (engine != NULL) {
		engine->h2_mitm_enabled = enabled ? 1 : 0;
	}
}

ANIXOPS_API void anixops_engine_set_disable_quic_for_mitm(anixops_engine_t *engine, int enabled)
{
	if (engine != NULL) {
		engine->disable_quic_for_mitm = enabled ? 1 : 0;
	}
}

ANIXOPS_API void anixops_engine_set_cert_state(anixops_engine_t *engine, anixops_cert_state_t state)
{
	if (engine != NULL) {
		engine->cert_state = state;
	}
}

ANIXOPS_API int anixops_mitm_evaluate(
	const anixops_engine_t *engine,
	const char *hostname,
	int is_quic,
	anixops_mitm_decision_t *out_decision)
{
	char host[512];
	size_t i;
	const char *allow_pattern = NULL;

	if (engine == NULL || hostname == NULL || out_decision == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}

	anixops_normalize_host(hostname, host, sizeof(host));
	if (!engine->mitm_enabled) {
		anixops_set_mitm_result(out_decision, ANIXOPS_MITM_BYPASS, ANIXOPS_MITM_REASON_DISABLED, NULL, "mitm disabled");
		return ANIXOPS_OK;
	}
	if (host[0] == '\0') {
		anixops_set_mitm_result(out_decision, ANIXOPS_MITM_BYPASS, ANIXOPS_MITM_REASON_EMPTY_HOST, NULL, "empty host");
		return ANIXOPS_OK;
	}
	if (engine->cert_state != ANIXOPS_CERT_TRUSTED) {
		anixops_set_mitm_result(
			out_decision,
			ANIXOPS_MITM_BYPASS,
			ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED,
			NULL,
			"mitm certificate not trusted");
		return ANIXOPS_OK;
	}

	for (i = 0; i < engine->host_len; i++) {
		if (engine->hosts[i].deny && anixops_host_pattern_matches(engine->hosts[i].pattern, host)) {
			anixops_set_mitm_result(
				out_decision,
				ANIXOPS_MITM_BYPASS,
				ANIXOPS_MITM_REASON_DENY_HOST,
				engine->hosts[i].pattern,
				"host matched deny pattern");
			return ANIXOPS_OK;
		}
	}

	for (i = 0; i < engine->host_len; i++) {
		if (!engine->hosts[i].deny && anixops_host_pattern_matches(engine->hosts[i].pattern, host)) {
			allow_pattern = engine->hosts[i].pattern;
			break;
		}
	}
	if (allow_pattern == NULL) {
		anixops_set_mitm_result(out_decision, ANIXOPS_MITM_BYPASS, ANIXOPS_MITM_REASON_NO_HOST_MATCH, NULL, "no mitm host match");
		return ANIXOPS_OK;
	}
	if (is_quic && engine->disable_quic_for_mitm) {
		anixops_set_mitm_result(
			out_decision,
			ANIXOPS_MITM_REJECT_QUIC,
			ANIXOPS_MITM_REASON_QUIC_DISABLED_FOR_MITM,
			allow_pattern,
			"quic should be rejected so tcp tls can be intercepted");
		return ANIXOPS_OK;
	}

	anixops_set_mitm_result(out_decision, ANIXOPS_MITM_INTERCEPT, ANIXOPS_MITM_REASON_ALLOWED, allow_pattern, "mitm allowed");
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_rewrite_evaluate_url(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	anixops_rewrite_result_t *out_result)
{
	size_t i;

	if (engine == NULL || url == NULL || out_result == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_set_rewrite_none(out_result);

	for (i = 0; i < engine->rule_len; i++) {
		const anixops_rewrite_rule_t *rule = &engine->rules[i];
		regmatch_t matches[10];
		int rc;
		if (rule->phase != phase || !rule->regex_ready || anixops_rewrite_action_rewrites_header(rule->action)) {
			continue;
		}
		rc = regexec(&rule->regex, url, sizeof(matches) / sizeof(matches[0]), matches, 0);
		if (rc != 0) {
			continue;
		}
		out_result->action = rule->action;
		out_result->status_code = rule->status_code;
		out_result->rule_index = (int)i;
		anixops_copy_text(out_result->matched_pattern, sizeof(out_result->matched_pattern), rule->pattern);
		if (anixops_rewrite_action_redirects(rule->action) ||
			rule->action == ANIXOPS_REWRITE_MOCK_REQUEST_BODY ||
			rule->action == ANIXOPS_REWRITE_MOCK_RESPONSE_BODY) {
			int expand_rc = anixops_expand_replacement(
				url,
				rule->replacement,
				matches,
				sizeof(matches) / sizeof(matches[0]),
				out_result->value,
				sizeof(out_result->value));
			if (expand_rc != ANIXOPS_OK) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "replacement truncated");
			}
		}
		else {
			out_result->value[0] = '\0';
		}
		if (out_result->message[0] == '\0') {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "rewrite matched");
		}
		return ANIXOPS_OK;
	}

	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_rewrite_apply_body(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	const char *body,
	char *out_body,
	size_t out_body_cap,
	anixops_rewrite_result_t *out_result)
{
	int rc;

	if (engine == NULL || url == NULL || body == NULL || out_body == NULL || out_body_cap == 0 ||
		out_result == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}

	rc = anixops_rewrite_evaluate_url(engine, url, phase, out_result);
	if (rc != ANIXOPS_OK) {
		return rc;
	}
	if (out_result->action == ANIXOPS_REWRITE_MOCK_REQUEST_BODY ||
		out_result->action == ANIXOPS_REWRITE_MOCK_RESPONSE_BODY) {
		if (anixops_copy_text_checked(out_body, out_body_cap, out_result->value) != ANIXOPS_OK) {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "body truncated");
		}
		return ANIXOPS_OK;
	}
	if (anixops_rewrite_action_replaces_body(out_result->action)) {
		int replaced = 0;
		const anixops_rewrite_rule_t *rule;
		if (out_result->rule_index < 0 || (size_t)out_result->rule_index >= engine->rule_len) {
			if (anixops_copy_text_checked(out_body, out_body_cap, body) != ANIXOPS_OK) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "body truncated");
			}
			return ANIXOPS_OK;
		}
		rule = &engine->rules[out_result->rule_index];
		if (anixops_rewrite_action_replaces_body_regex(out_result->action)) {
			if (!rule->body_regex_ready ||
				anixops_apply_body_regex_replacement(body, rule, out_body, out_body_cap, &replaced) != ANIXOPS_OK) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "body truncated");
				return ANIXOPS_OK;
			}
			if (replaced) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "body rewritten");
			}
			else {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "body regex not matched");
			}
			return ANIXOPS_OK;
		}
		if (anixops_rewrite_action_replaces_body_json(out_result->action)) {
			int path_supported = 1;
			if (anixops_apply_body_json_replacement(
					body,
					rule,
					out_body,
					out_body_cap,
					&replaced,
					&path_supported) != ANIXOPS_OK) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "body truncated");
				return ANIXOPS_OK;
			}
			if (!path_supported) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "json path unsupported");
			}
			else if (replaced) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "json body rewritten");
			}
			else {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "json path not matched");
			}
			return ANIXOPS_OK;
		}
		return ANIXOPS_OK;
	}

	if (anixops_copy_text_checked(out_body, out_body_cap, body) != ANIXOPS_OK) {
		anixops_copy_text(out_result->message, sizeof(out_result->message), "body truncated");
	}
	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_rewrite_evaluate_header(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	size_t start_index,
	const char *current_header_value,
	anixops_header_rewrite_result_t *out_result)
{
	size_t i;

	if (engine == NULL || url == NULL || out_result == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_set_header_rewrite_none(out_result);

	for (i = start_index; i < engine->rule_len; i++) {
		const anixops_rewrite_rule_t *rule = &engine->rules[i];
		regmatch_t url_matches[10];
		int rc;
		if (rule->phase != phase || !rule->regex_ready || !anixops_rewrite_action_rewrites_header(rule->action)) {
			continue;
		}
		rc = regexec(&rule->regex, url, sizeof(url_matches) / sizeof(url_matches[0]), url_matches, 0);
		if (rc != 0) {
			continue;
		}

		out_result->action = rule->action;
		out_result->phase = rule->phase;
		out_result->rule_index = (int)i;
		anixops_copy_text(out_result->matched_pattern, sizeof(out_result->matched_pattern), rule->pattern);
		anixops_copy_text(out_result->header_name, sizeof(out_result->header_name), rule->header_name);

		if (anixops_rewrite_action_replaces_header_regex(rule->action)) {
			int replaced = 0;
			if (current_header_value == NULL) {
				anixops_copy_text(out_result->value, sizeof(out_result->value), rule->replacement);
				anixops_copy_text(out_result->message, sizeof(out_result->message), "header regex value required");
				return ANIXOPS_OK;
			}
			if (!rule->header_regex_ready ||
				anixops_apply_regex_replacement(
					current_header_value,
					&rule->header_regex,
					rule->header_regex_ready,
					rule->replacement,
					out_result->value,
					sizeof(out_result->value),
					&replaced) != ANIXOPS_OK) {
				anixops_copy_text(out_result->message, sizeof(out_result->message), "header value truncated");
				return ANIXOPS_OK;
			}
			anixops_copy_text(
				out_result->message,
				sizeof(out_result->message),
				replaced ? "header rewritten" : "header regex not matched");
			return ANIXOPS_OK;
		}

		if (rule->action == ANIXOPS_REWRITE_RESPONSE_HEADER_DEL) {
			out_result->value[0] = '\0';
			anixops_copy_text(out_result->message, sizeof(out_result->message), "header rewrite matched");
			return ANIXOPS_OK;
		}

		if (anixops_expand_replacement(
				url,
				rule->replacement,
				url_matches,
				sizeof(url_matches) / sizeof(url_matches[0]),
				out_result->value,
				sizeof(out_result->value)) != ANIXOPS_OK) {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "header value truncated");
		}
		else {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "header rewrite matched");
		}
		return ANIXOPS_OK;
	}

	return ANIXOPS_OK;
}

ANIXOPS_API int anixops_script_evaluate_url(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	anixops_script_result_t *out_result)
{
	size_t i;

	if (engine == NULL || url == NULL || out_result == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	anixops_set_script_none(out_result);

	for (i = 0; i < engine->script_len; i++) {
		const anixops_script_rule_t *rule = &engine->scripts[i];
		int rc;
		if (rule->phase != phase || !rule->regex_ready) {
			continue;
		}
		rc = regexec(&rule->regex, url, 0, NULL, 0);
		if (rc != 0) {
			continue;
		}
		out_result->kind = rule->kind;
		out_result->phase = rule->phase;
		out_result->requires_body = rule->requires_body;
		out_result->rule_index = (int)i;
		anixops_copy_text(out_result->matched_pattern, sizeof(out_result->matched_pattern), rule->pattern);
		anixops_copy_text(out_result->script_path, sizeof(out_result->script_path), rule->script_path);
		anixops_copy_text(out_result->tag, sizeof(out_result->tag), rule->tag);
		if (anixops_resolve_script_argument(
				engine,
				rule->argument_template,
				out_result->argument,
				sizeof(out_result->argument)) != ANIXOPS_OK) {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "script argument truncated");
		}
		else {
			anixops_copy_text(out_result->message, sizeof(out_result->message), "script matched");
		}
		return ANIXOPS_OK;
	}

	return ANIXOPS_OK;
}

ANIXOPS_API size_t anixops_engine_mitm_pattern_count(const anixops_engine_t *engine)
{
	return engine == NULL ? 0 : engine->host_len;
}

ANIXOPS_API size_t anixops_engine_rewrite_rule_count(const anixops_engine_t *engine)
{
	return engine == NULL ? 0 : engine->rule_len;
}

ANIXOPS_API size_t anixops_engine_script_rule_count(const anixops_engine_t *engine)
{
	return engine == NULL ? 0 : engine->script_len;
}

ANIXOPS_API size_t anixops_engine_argument_count(const anixops_engine_t *engine)
{
	return engine == NULL ? 0 : engine->argument_len;
}

ANIXOPS_API int anixops_engine_h2_mitm_enabled(const anixops_engine_t *engine)
{
	return engine == NULL ? 0 : engine->h2_mitm_enabled;
}

static char *anixops_strdup(const char *value)
{
	if (value == NULL) {
		return NULL;
	}
	return anixops_strndup(value, strlen(value));
}

static char *anixops_strndup(const char *value, size_t len)
{
	char *copy = (char *)malloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}
	memcpy(copy, value, len);
	copy[len] = '\0';
	return copy;
}

static void anixops_free_rewrite_rule(anixops_rewrite_rule_t *rule)
{
	if (rule == NULL) {
		return;
	}
	if (rule->regex_ready) {
		regfree(&rule->regex);
	}
	if (rule->body_regex_ready) {
		regfree(&rule->body_regex);
	}
	if (rule->header_regex_ready) {
		regfree(&rule->header_regex);
	}
	free(rule->source);
	free(rule->pattern);
	free(rule->replacement);
	free(rule->body_pattern);
	free(rule->header_name);
	free(rule->header_pattern);
	memset(rule, 0, sizeof(*rule));
}

static void anixops_free_script_rule(anixops_script_rule_t *rule)
{
	if (rule == NULL) {
		return;
	}
	if (rule->regex_ready) {
		regfree(&rule->regex);
	}
	free(rule->source);
	free(rule->pattern);
	free(rule->script_path);
	free(rule->tag);
	free(rule->argument_template);
	memset(rule, 0, sizeof(*rule));
}

static void anixops_free_argument(anixops_argument_t *argument)
{
	if (argument == NULL) {
		return;
	}
	free(argument->name);
	free(argument->default_value);
	free(argument->value);
	memset(argument, 0, sizeof(*argument));
}

static int anixops_reserve_hosts(anixops_engine_t *engine, size_t want)
{
	anixops_host_pattern_t *next;
	size_t next_cap;
	if (want <= engine->host_cap) {
		return ANIXOPS_OK;
	}
	next_cap = engine->host_cap == 0 ? 8 : engine->host_cap * 2;
	while (next_cap < want) {
		next_cap *= 2;
	}
	next = (anixops_host_pattern_t *)realloc(engine->hosts, next_cap * sizeof(*next));
	if (next == NULL) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	engine->hosts = next;
	engine->host_cap = next_cap;
	return ANIXOPS_OK;
}

static int anixops_reserve_rules(anixops_engine_t *engine, size_t want)
{
	anixops_rewrite_rule_t *next;
	size_t next_cap;
	if (want <= engine->rule_cap) {
		return ANIXOPS_OK;
	}
	next_cap = engine->rule_cap == 0 ? 8 : engine->rule_cap * 2;
	while (next_cap < want) {
		next_cap *= 2;
	}
	next = (anixops_rewrite_rule_t *)realloc(engine->rules, next_cap * sizeof(*next));
	if (next == NULL) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	memset(next + engine->rule_cap, 0, (next_cap - engine->rule_cap) * sizeof(*next));
	engine->rules = next;
	engine->rule_cap = next_cap;
	return ANIXOPS_OK;
}

static int anixops_reserve_scripts(anixops_engine_t *engine, size_t want)
{
	anixops_script_rule_t *next;
	size_t next_cap;
	if (want <= engine->script_cap) {
		return ANIXOPS_OK;
	}
	next_cap = engine->script_cap == 0 ? 8 : engine->script_cap * 2;
	while (next_cap < want) {
		next_cap *= 2;
	}
	next = (anixops_script_rule_t *)realloc(engine->scripts, next_cap * sizeof(*next));
	if (next == NULL) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	memset(next + engine->script_cap, 0, (next_cap - engine->script_cap) * sizeof(*next));
	engine->scripts = next;
	engine->script_cap = next_cap;
	return ANIXOPS_OK;
}

static int anixops_reserve_arguments(anixops_engine_t *engine, size_t want)
{
	anixops_argument_t *next;
	size_t next_cap;
	if (want <= engine->argument_cap) {
		return ANIXOPS_OK;
	}
	next_cap = engine->argument_cap == 0 ? 8 : engine->argument_cap * 2;
	while (next_cap < want) {
		next_cap *= 2;
	}
	next = (anixops_argument_t *)realloc(engine->arguments, next_cap * sizeof(*next));
	if (next == NULL) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	memset(next + engine->argument_cap, 0, (next_cap - engine->argument_cap) * sizeof(*next));
	engine->arguments = next;
	engine->argument_cap = next_cap;
	return ANIXOPS_OK;
}

static void anixops_trim_inplace(char *value)
{
	char *start;
	char *end;
	if (value == NULL) {
		return;
	}
	start = value;
	while (*start != '\0' && isspace((unsigned char)*start)) {
		start++;
	}
	if (start != value) {
		memmove(value, start, strlen(start) + 1);
	}
	end = value + strlen(value);
	while (end > value && isspace((unsigned char)*(end - 1))) {
		end--;
	}
	*end = '\0';
}

static void anixops_lower_inplace(char *value)
{
	if (value == NULL) {
		return;
	}
	while (*value != '\0') {
		*value = (char)tolower((unsigned char)*value);
		value++;
	}
}

static int anixops_parse_bool(const char *value)
{
	if (value == NULL) {
		return 0;
	}
	return strcasecmp(value, "true") == 0 ||
		strcasecmp(value, "yes") == 0 ||
		strcasecmp(value, "on") == 0 ||
		strcmp(value, "1") == 0;
}

static int anixops_starts_with_ci(const char *value, const char *prefix)
{
	size_t prefix_len = strlen(prefix);
	return strncasecmp(value, prefix, prefix_len) == 0;
}

static int anixops_next_token(const char **cursor, char *out, size_t out_cap)
{
	const char *p;
	size_t pos = 0;
	char quote = '\0';
	if (cursor == NULL || *cursor == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	p = *cursor;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') {
		out[0] = '\0';
		*cursor = p;
		return 0;
	}
	if (*p == '"' || *p == '\'') {
		quote = *p;
		p++;
	}
	while (*p != '\0') {
		char ch = *p;
		if (quote != '\0') {
			if (ch == quote) {
				p++;
				break;
			}
			if (ch == '\\' && p[1] != '\0' && (p[1] == quote || p[1] == '\\')) {
				p++;
				ch = *p;
			}
		}
		else if (isspace((unsigned char)ch)) {
			break;
		}
		if (pos + 1 < out_cap) {
			out[pos++] = ch;
		}
		p++;
	}
	out[pos] = '\0';
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	*cursor = p;
	return 1;
}

static int anixops_next_csv_field(const char **cursor, char *out, size_t out_cap)
{
	const char *p;
	size_t pos = 0;
	char quote = '\0';
	if (cursor == NULL || *cursor == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	p = *cursor;
	while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) {
		p++;
	}
	if (*p == '\0') {
		out[0] = '\0';
		*cursor = p;
		return 0;
	}
	while (*p != '\0') {
		char ch = *p;
		if (quote != '\0') {
			if (ch == quote) {
				quote = '\0';
			}
			else if (ch == '\\' && p[1] != '\0') {
				if (pos + 1 < out_cap) {
					out[pos++] = ch;
				}
				p++;
				ch = *p;
			}
		}
		else {
			if (ch == '"' || ch == '\'') {
				quote = ch;
			}
			else if (ch == ',') {
				break;
			}
		}
		if (pos + 1 < out_cap) {
			out[pos++] = ch;
		}
		p++;
	}
	out[pos] = '\0';
	anixops_trim_inplace(out);
	if (*p == ',') {
		p++;
	}
	*cursor = p;
	return 1;
}

static int anixops_compile_regex(regex_t *regex, const char *pattern, int flags)
{
	const char *compile_pattern;
	char *normalized_pattern = NULL;
	int rc;
	if (regex == NULL || pattern == NULL) {
		return -1;
	}
	compile_pattern = anixops_regex_pattern_after_inline_flags(pattern, &flags);
	if (anixops_normalize_regex_pattern(compile_pattern, &normalized_pattern) != ANIXOPS_OK) {
		return REG_ESPACE;
	}
	rc = regcomp(regex, normalized_pattern, flags);
	free(normalized_pattern);
	return rc;
}

static const char *anixops_regex_pattern_after_inline_flags(const char *pattern, int *flags)
{
	if (pattern != NULL && flags != NULL && strncmp(pattern, "(?i)", 4) == 0) {
		*flags |= REG_ICASE;
		return pattern + 4;
	}
	return pattern;
}

static int anixops_normalize_regex_pattern(const char *pattern, char **out_pattern)
{
	size_t len;
	size_t cap;
	size_t pos = 0;
	size_t i;
	int in_class = 0;
	char *out;
	if (pattern == NULL || out_pattern == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	len = strlen(pattern);
	if (len > ((size_t)-1 - 1) / 16) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	cap = len * 16 + 1;
	out = (char *)malloc(cap);
	if (out == NULL) {
		return ANIXOPS_ERR_OUT_OF_MEMORY;
	}
	for (i = 0; i < len; i++) {
		char ch = pattern[i];
		const char *replacement = NULL;
		if (ch == '\\' && i + 1 < len) {
			char next = pattern[i + 1];
			if (next == 'd') {
				replacement = in_class ? "0-9" : "[0-9]";
			}
			else if (next == 'D' && !in_class) {
				replacement = "[^0-9]";
			}
			else if (next == 'w') {
				replacement = in_class ? "A-Za-z0-9_" : "[A-Za-z0-9_]";
			}
			else if (next == 'W' && !in_class) {
				replacement = "[^A-Za-z0-9_]";
			}
			else if (next == 's') {
				replacement = in_class ? "[:space:]" : "[[:space:]]";
			}
			else if (next == 'S' && !in_class) {
				replacement = "[^[:space:]]";
			}
			else if (next == 't' || next == 'n' || next == 'r' || next == 'f' || next == 'a' || next == 'e') {
				char value;
				switch (next) {
				case 't':
					value = '\t';
					break;
				case 'n':
					value = '\n';
					break;
				case 'r':
					value = '\r';
					break;
				case 'f':
					value = '\f';
					break;
				case 'a':
					value = '\a';
					break;
				default:
					value = 0x1b;
					break;
				}
				if (anixops_regex_append_literal_bytes(out, cap, &pos, &value, 1, in_class) != ANIXOPS_OK) {
					free(out);
					return ANIXOPS_ERR_PARSE;
				}
				i++;
				continue;
			}
			else if (next == 'x' && i + 3 < len) {
				int hi = anixops_hex_digit((unsigned char)pattern[i + 2]);
				int lo = anixops_hex_digit((unsigned char)pattern[i + 3]);
				if (hi >= 0 && lo >= 0) {
					unsigned char value = (unsigned char)((hi << 4) | lo);
					if (value == '\0') {
						free(out);
						return ANIXOPS_ERR_PARSE;
					}
					if (anixops_regex_append_literal_bytes(out, cap, &pos, (const char *)&value, 1, in_class) !=
						ANIXOPS_OK) {
						free(out);
						return ANIXOPS_ERR_PARSE;
					}
					i += 3;
					continue;
				}
			}
			else if (next == 'u' && i + 5 < len) {
				const char *cursor = pattern + i + 2;
				const char *end = pattern + len;
				unsigned long codepoint;
				char utf8[4];
				size_t utf8_len = 0;
				int valid = 1;
				if (anixops_json_read_hex4(&cursor, end, &codepoint)) {
					if (codepoint >= 0xD800UL && codepoint <= 0xDBFFUL) {
						unsigned long low;
						if ((size_t)(end - cursor) < 6 || cursor[0] != '\\' || cursor[1] != 'u') {
							valid = 0;
						}
						else {
							cursor += 2;
							if (!anixops_json_read_hex4(&cursor, end, &low) || low < 0xDC00UL || low > 0xDFFFUL) {
								valid = 0;
							}
							else {
								codepoint = 0x10000UL + ((codepoint - 0xD800UL) << 10) + (low - 0xDC00UL);
							}
						}
					}
					else if (codepoint >= 0xDC00UL && codepoint <= 0xDFFFUL) {
						valid = 0;
					}
					if (valid) {
						if (codepoint == 0 || !anixops_json_append_utf8(utf8, sizeof(utf8), &utf8_len, codepoint) ||
							anixops_regex_append_literal_bytes(out, cap, &pos, utf8, utf8_len, in_class) != ANIXOPS_OK) {
							free(out);
							return ANIXOPS_ERR_PARSE;
						}
						i = (size_t)(cursor - pattern) - 1;
						continue;
					}
				}
			}
			else if (next == 'A' && !in_class) {
				replacement = "^";
			}
			else if ((next == 'z' || next == 'Z') && !in_class) {
				replacement = "$";
			}
			else if (next == 'Q' && !in_class) {
				size_t q = i + 2;
				while (q < len) {
					char literal = pattern[q];
					if (literal == '\\' && q + 1 < len && pattern[q + 1] == 'E') {
						break;
					}
					if (strchr(".^$*+?()[]{}|\\", literal) != NULL) {
						out[pos++] = '\\';
					}
					out[pos++] = literal;
					q++;
				}
				i = (q + 1 < len && pattern[q] == '\\' && pattern[q + 1] == 'E') ? q + 1 : len - 1;
				continue;
			}
			if (replacement != NULL) {
				size_t replacement_len = strlen(replacement);
				memcpy(out + pos, replacement, replacement_len);
				pos += replacement_len;
				i++;
				continue;
			}
			out[pos++] = ch;
			i++;
			out[pos++] = pattern[i];
			continue;
		}
		if (!in_class && ch == '?' && i > 0 && !anixops_regex_char_is_escaped(pattern, i - 1) &&
			(pattern[i - 1] == '*' || pattern[i - 1] == '+' || pattern[i - 1] == '?' ||
				(pattern[i - 1] == '}' && anixops_regex_closes_interval_quantifier(pattern, i - 1)))) {
			continue;
		}
		if (!in_class && ch == '(' && i + 2 < len && pattern[i + 1] == '?' && pattern[i + 2] == ':') {
			out[pos++] = '(';
			i += 2;
			continue;
		}
		if (!in_class && ch == '(' && i + 3 < len && pattern[i + 1] == '?' &&
			(pattern[i + 2] == '<' || pattern[i + 2] == '\'')) {
			char terminator = pattern[i + 2] == '<' ? '>' : '\'';
			size_t name_start = i + 3;
			size_t name_end = name_start;
			if (pattern[name_start] != '=' && pattern[name_start] != '!') {
				while (name_end < len && pattern[name_end] != terminator &&
					(isalnum((unsigned char)pattern[name_end]) || pattern[name_end] == '_')) {
					name_end++;
				}
				if (name_end > name_start && name_end < len && pattern[name_end] == terminator) {
					out[pos++] = '(';
					i = name_end;
					continue;
				}
			}
		}
		if (ch == '[' && !in_class) {
			in_class = 1;
		}
		else if (ch == ']' && in_class) {
			in_class = 0;
		}
		out[pos++] = ch;
	}
	out[pos] = '\0';
	*out_pattern = out;
	return ANIXOPS_OK;
}

static int anixops_regex_append_literal_bytes(
	char *out,
	size_t out_cap,
	size_t *pos,
	const char *bytes,
	size_t len,
	int in_class)
{
	const char *meta = in_class ? "\\]-^" : ".^$*+?()[]{}|\\";
	size_t i;
	if (out == NULL || pos == NULL || bytes == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	for (i = 0; i < len; i++) {
		unsigned char value = (unsigned char)bytes[i];
		if (value == '\0') {
			return ANIXOPS_ERR_PARSE;
		}
		if (value < 0x80 && strchr(meta, value) != NULL) {
			if (*pos + 1 >= out_cap) {
				return ANIXOPS_ERR_CAPACITY;
			}
			out[(*pos)++] = '\\';
		}
		if (*pos + 1 >= out_cap) {
			return ANIXOPS_ERR_CAPACITY;
		}
		out[(*pos)++] = (char)value;
	}
	return ANIXOPS_OK;
}

static int anixops_regex_char_is_escaped(const char *pattern, size_t index)
{
	size_t slash_count = 0;
	if (pattern == NULL || index == 0) {
		return 0;
	}
	while (index > 0 && pattern[index - 1] == '\\') {
		slash_count++;
		index--;
	}
	return (slash_count % 2) != 0;
}

static int anixops_regex_closes_interval_quantifier(const char *pattern, size_t close_index)
{
	size_t cursor;
	int saw_digit = 0;
	if (pattern == NULL || pattern[close_index] != '}' || anixops_regex_char_is_escaped(pattern, close_index)) {
		return 0;
	}
	cursor = close_index;
	while (cursor > 0) {
		unsigned char ch;
		cursor--;
		ch = (unsigned char)pattern[cursor];
		if (ch == '{' && !anixops_regex_char_is_escaped(pattern, cursor)) {
			return saw_digit;
		}
		if (!(isdigit(ch) || ch == ',')) {
			return 0;
		}
		if (isdigit(ch)) {
			saw_digit = 1;
		}
	}
	return 0;
}

static int anixops_parse_rewrite_action(const char *token, anixops_rewrite_action_t *action, int *status_code)
{
	if (token == NULL || action == NULL || status_code == NULL) {
		return 0;
	}
	if (strcmp(token, "301") == 0) {
		*action = ANIXOPS_REWRITE_REDIRECT_301;
		*status_code = 301;
		return 1;
	}
	if (strcmp(token, "302") == 0) {
		*action = ANIXOPS_REWRITE_REDIRECT_302;
		*status_code = 302;
		return 1;
	}
	if (strcmp(token, "303") == 0) {
		*action = ANIXOPS_REWRITE_REDIRECT_303;
		*status_code = 303;
		return 1;
	}
	if (strcmp(token, "307") == 0) {
		*action = ANIXOPS_REWRITE_REDIRECT_307;
		*status_code = 307;
		return 1;
	}
	if (strcmp(token, "308") == 0) {
		*action = ANIXOPS_REWRITE_REDIRECT_308;
		*status_code = 308;
		return 1;
	}
	if (strcasecmp(token, "reject") == 0 || strcasecmp(token, "reject-current-session") == 0) {
		*action = ANIXOPS_REWRITE_REJECT;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "reject-200") == 0) {
		*action = ANIXOPS_REWRITE_REJECT_200;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "reject-401") == 0) {
		*action = ANIXOPS_REWRITE_REJECT;
		*status_code = 401;
		return 1;
	}
	if (strncasecmp(token, "reject-", 7) == 0 &&
		isdigit((unsigned char)token[7]) &&
		isdigit((unsigned char)token[8]) &&
		isdigit((unsigned char)token[9]) &&
		token[10] == '\0') {
		int code = (token[7] - '0') * 100 + (token[8] - '0') * 10 + (token[9] - '0');
		if (code >= 100 && code <= 599) {
			*action = ANIXOPS_REWRITE_REJECT;
			*status_code = code;
			return 1;
		}
	}
	if (strcasecmp(token, "reject-img") == 0) {
		*action = ANIXOPS_REWRITE_REJECT_IMG;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "reject-video") == 0) {
		*action = ANIXOPS_REWRITE_REJECT_VIDEO;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "reject-dict") == 0) {
		*action = ANIXOPS_REWRITE_REJECT_DICT;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "reject-array") == 0) {
		*action = ANIXOPS_REWRITE_REJECT_ARRAY;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "mock-request-body") == 0) {
		*action = ANIXOPS_REWRITE_MOCK_REQUEST_BODY;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "mock-response-body") == 0) {
		*action = ANIXOPS_REWRITE_MOCK_RESPONSE_BODY;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "request-body-replace-regex") == 0) {
		*action = ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-body-replace-regex") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "request-body-json-replace") == 0) {
		*action = ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-body-json-replace") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE;
		*status_code = 200;
		return 1;
	}
	if (strcasecmp(token, "header-replace") == 0) {
		*action = ANIXOPS_REWRITE_HEADER_REPLACE;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "header-add") == 0) {
		*action = ANIXOPS_REWRITE_HEADER_ADD;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "header-replace-regex") == 0) {
		*action = ANIXOPS_REWRITE_HEADER_REPLACE_REGEX;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-header-del") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_HEADER_DEL;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-header-replace") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-header-add") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_HEADER_ADD;
		*status_code = 0;
		return 1;
	}
	if (strcasecmp(token, "response-header-replace-regex") == 0) {
		*action = ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX;
		*status_code = 0;
		return 1;
	}
	return 0;
}

static int anixops_rewrite_action_redirects(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_REDIRECT_301 ||
		action == ANIXOPS_REWRITE_REDIRECT_302 ||
		action == ANIXOPS_REWRITE_REDIRECT_303 ||
		action == ANIXOPS_REWRITE_REDIRECT_307 ||
		action == ANIXOPS_REWRITE_REDIRECT_308;
}

static int anixops_rewrite_action_replaces_body(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE;
}

static int anixops_rewrite_action_replaces_body_regex(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX;
}

static int anixops_rewrite_action_replaces_body_json(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE;
}

static int anixops_rewrite_action_rewrites_header(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_HEADER_REPLACE ||
		action == ANIXOPS_REWRITE_HEADER_ADD ||
		action == ANIXOPS_REWRITE_HEADER_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_DEL ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_ADD ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX;
}

static int anixops_rewrite_action_replaces_header_regex(anixops_rewrite_action_t action)
{
	return action == ANIXOPS_REWRITE_HEADER_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX;
}

static int anixops_parse_script_kind(const char *token, anixops_script_kind_t *kind, anixops_phase_t *phase)
{
	if (token == NULL || kind == NULL || phase == NULL) {
		return 0;
	}
	if (strcasecmp(token, "http-request") == 0 || strcasecmp(token, "script-request-body") == 0 ||
		strcasecmp(token, "script-request-header") == 0) {
		*kind = ANIXOPS_SCRIPT_HTTP_REQUEST;
		*phase = ANIXOPS_PHASE_REQUEST;
		return 1;
	}
	if (strcasecmp(token, "http-response") == 0 || strcasecmp(token, "script-response-body") == 0 ||
		strcasecmp(token, "script-response-header") == 0) {
		*kind = ANIXOPS_SCRIPT_HTTP_RESPONSE;
		*phase = ANIXOPS_PHASE_RESPONSE;
		return 1;
	}
	return 0;
}

static int anixops_script_kind_requires_body_token(const char *token)
{
	const char *needle = "body";
	size_t needle_len = strlen(needle);
	if (token == NULL) {
		return 0;
	}
	while (*token != '\0') {
		if (strncasecmp(token, needle, needle_len) == 0) {
			return 1;
		}
		token++;
	}
	return 0;
}

static anixops_phase_t anixops_default_phase_for_action(anixops_rewrite_action_t action)
{
	if (action == ANIXOPS_REWRITE_MOCK_RESPONSE_BODY ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX ||
		action == ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_DEL ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_ADD ||
		action == ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX) {
		return ANIXOPS_PHASE_RESPONSE;
	}
	return ANIXOPS_PHASE_REQUEST;
}

static int anixops_extract_attr(const char *attrs, const char *key, char *out, size_t out_cap)
{
	const char *segment_start;
	const char *p;
	char quote = '\0';
	int bracket_depth = 0;
	if (attrs == NULL || key == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	segment_start = attrs;
	p = attrs;
	while (1) {
		char ch = *p;
		int at_end = ch == '\0';
		if (!at_end) {
			if (quote != '\0') {
				if (ch == quote) {
					quote = '\0';
				}
				else if (ch == '\\' && p[1] != '\0') {
					p++;
				}
			}
			else {
				if (ch == '"' || ch == '\'') {
					quote = ch;
				}
				else if (ch == '[' || ch == '(' || ch == '{') {
					bracket_depth++;
				}
				else if ((ch == ']' || ch == ')' || ch == '}') && bracket_depth > 0) {
					bracket_depth--;
				}
				else if (ch == ',' && bracket_depth == 0) {
					at_end = 1;
				}
			}
		}
		if (at_end) {
			char segment[4096];
			char parsed_key[256];
			char parsed_value[4096];
			size_t len = (size_t)(p - segment_start);
			if (len >= sizeof(segment)) {
				len = sizeof(segment) - 1;
			}
			memcpy(segment, segment_start, len);
			segment[len] = '\0';
			if (anixops_split_attr_segment(segment, parsed_key, sizeof(parsed_key), parsed_value, sizeof(parsed_value)) &&
				strcasecmp(parsed_key, key) == 0) {
				anixops_copy_text(out, out_cap, parsed_value);
				return 1;
			}
			if (ch == '\0') {
				break;
			}
			p++;
			segment_start = p;
			continue;
		}
		p++;
	}
	return 0;
}

static int anixops_split_attr_segment(const char *segment, char *key, size_t key_cap, char *value, size_t value_cap)
{
	const char *eq;
	size_t key_len;
	if (segment == NULL || key == NULL || value == NULL || key_cap == 0 || value_cap == 0) {
		return 0;
	}
	eq = strchr(segment, '=');
	if (eq == NULL) {
		return 0;
	}
	key_len = (size_t)(eq - segment);
	if (key_len >= key_cap) {
		key_len = key_cap - 1;
	}
	memcpy(key, segment, key_len);
	key[key_len] = '\0';
	anixops_trim_inplace(key);
	anixops_copy_text(value, value_cap, eq + 1);
	anixops_trim_inplace(value);
	return key[0] != '\0';
}

static void anixops_unquote_inplace(char *value)
{
	size_t len;
	char quote;
	if (value == NULL) {
		return;
	}
	anixops_trim_inplace(value);
	len = strlen(value);
	if (len < 2) {
		return;
	}
	quote = value[0];
	if ((quote == '"' || quote == '\'') && value[len - 1] == quote) {
		size_t i;
		size_t pos = 0;
		for (i = 1; i + 1 < len; i++) {
			if (value[i] == '\\' && i + 2 < len) {
				i++;
			}
			value[pos++] = value[i];
		}
		value[pos] = '\0';
	}
}

static int anixops_argument_index(const anixops_engine_t *engine, const char *name)
{
	size_t i;
	if (engine == NULL || name == NULL) {
		return -1;
	}
	for (i = 0; i < engine->argument_len; i++) {
		if (strcmp(engine->arguments[i].name, name) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static const char *anixops_argument_value(const anixops_engine_t *engine, const char *name)
{
	int idx = anixops_argument_index(engine, name);
	if (idx < 0) {
		return "";
	}
	if (engine->arguments[idx].value != NULL) {
		return engine->arguments[idx].value;
	}
	return engine->arguments[idx].default_value == NULL ? "" : engine->arguments[idx].default_value;
}

static int anixops_resolve_script_argument(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap)
{
	const char *p;
	if (out == NULL || out_cap == 0) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	out[0] = '\0';
	if (argument_template == NULL || argument_template[0] == '\0') {
		return ANIXOPS_OK;
	}
	p = argument_template;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '[') {
		return anixops_resolve_argument_list(engine, argument_template, out, out_cap);
	}
	return anixops_resolve_argument_template(engine, argument_template, out, out_cap);
}

static int anixops_resolve_argument_list(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap)
{
	const char *p = argument_template;
	size_t pos = 0;
	int first = 1;
	int truncated = 0;
	while (*p != '\0') {
		if (*p == '{') {
			const char *end = strchr(p + 1, '}');
			char name[256];
			const char *value;
			size_t len;
			if (end == NULL) {
				break;
			}
			len = (size_t)(end - (p + 1));
			if (len >= sizeof(name)) {
				len = sizeof(name) - 1;
			}
			memcpy(name, p + 1, len);
			name[len] = '\0';
			anixops_trim_inplace(name);
			value = anixops_argument_value(engine, name);
			if (!first && anixops_append_char(out, out_cap, &pos, '&') != ANIXOPS_OK) {
				truncated = 1;
			}
			first = 0;
			if (anixops_append_range(out, out_cap, &pos, name, strlen(name)) != ANIXOPS_OK ||
				anixops_append_char(out, out_cap, &pos, '=') != ANIXOPS_OK ||
				anixops_append_range(out, out_cap, &pos, value, strlen(value)) != ANIXOPS_OK) {
				truncated = 1;
			}
			p = end + 1;
			continue;
		}
		p++;
	}
	if (out_cap > 0) {
		out[pos < out_cap ? pos : out_cap - 1] = '\0';
	}
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_resolve_argument_template(
	const anixops_engine_t *engine,
	const char *argument_template,
	char *out,
	size_t out_cap)
{
	const char *p = argument_template;
	size_t pos = 0;
	int truncated = 0;
	while (*p != '\0') {
		if (p[0] == '{' && p[1] == '{' && p[2] == '{') {
			const char *end = strstr(p + 3, "}}}");
			char name[256];
			const char *value;
			size_t len;
			if (end == NULL) {
				if (anixops_append_char(out, out_cap, &pos, *p) != ANIXOPS_OK) {
					truncated = 1;
				}
				p++;
				continue;
			}
			len = (size_t)(end - (p + 3));
			if (len >= sizeof(name)) {
				len = sizeof(name) - 1;
			}
			memcpy(name, p + 3, len);
			name[len] = '\0';
			anixops_trim_inplace(name);
			value = anixops_argument_value(engine, name);
			if (anixops_append_range(out, out_cap, &pos, value, strlen(value)) != ANIXOPS_OK) {
				truncated = 1;
			}
			p = end + 3;
			continue;
		}
		if (*p == '{') {
			const char *end = strchr(p + 1, '}');
			char name[256];
			const char *value;
			size_t len;
			if (end == NULL) {
				if (anixops_append_char(out, out_cap, &pos, *p) != ANIXOPS_OK) {
					truncated = 1;
				}
				p++;
				continue;
			}
			len = (size_t)(end - (p + 1));
			if (len >= sizeof(name)) {
				len = sizeof(name) - 1;
			}
			memcpy(name, p + 1, len);
			name[len] = '\0';
			anixops_trim_inplace(name);
			value = anixops_argument_value(engine, name);
			if (anixops_append_range(out, out_cap, &pos, value, strlen(value)) != ANIXOPS_OK) {
				truncated = 1;
			}
			p = end + 1;
			continue;
		}
		if (anixops_append_char(out, out_cap, &pos, *p) != ANIXOPS_OK) {
			truncated = 1;
		}
		p++;
	}
	if (out_cap > 0) {
		out[pos < out_cap ? pos : out_cap - 1] = '\0';
	}
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_expand_replacement(
	const char *input,
	const char *replacement,
	const regmatch_t *matches,
	size_t match_count,
	char *out,
	size_t out_cap)
{
	size_t pos = 0;
	const char *p = replacement == NULL ? "" : replacement;
	int truncated = 0;
	if (input == NULL || out == NULL || out_cap == 0) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	while (*p != '\0') {
		if ((*p == '$' || *p == '\\') && isdigit((unsigned char)p[1])) {
			size_t idx = (size_t)(p[1] - '0');
			if (idx < match_count && matches[idx].rm_so >= 0 && matches[idx].rm_eo >= matches[idx].rm_so) {
				size_t start = (size_t)matches[idx].rm_so;
				size_t len = (size_t)(matches[idx].rm_eo - matches[idx].rm_so);
				if (anixops_append_range(out, out_cap, &pos, input + start, len) != ANIXOPS_OK) {
					truncated = 1;
				}
			}
			p += 2;
			continue;
		}
		if (*p == '\\' && p[1] == '/') {
			if (anixops_append_char(out, out_cap, &pos, '/') != ANIXOPS_OK) {
				truncated = 1;
			}
			p += 2;
			continue;
		}
		if (anixops_append_char(out, out_cap, &pos, *p) != ANIXOPS_OK) {
			truncated = 1;
		}
		p++;
	}
	if (out_cap > 0) {
		out[pos < out_cap ? pos : out_cap - 1] = '\0';
	}
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_apply_body_regex_replacement(
	const char *body,
	const anixops_rewrite_rule_t *rule,
	char *out,
	size_t out_cap,
	int *out_replaced)
{
	if (rule == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	return anixops_apply_regex_replacement(
		body,
		&rule->body_regex,
		rule->body_regex_ready,
		rule->replacement,
		out,
		out_cap,
		out_replaced);
}

static int anixops_apply_body_json_replacement(
	const char *body,
	const anixops_rewrite_rule_t *rule,
	char *out,
	size_t out_cap,
	int *out_replaced,
	int *out_path_supported)
{
	const char *value_start;
	const char *value_end;
	size_t pos = 0;
	int found;
	int truncated = 0;

	if (body == NULL || rule == NULL || out == NULL || out_cap == 0 || out_replaced == NULL ||
		out_path_supported == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	*out_replaced = 0;
	*out_path_supported = 1;

	found = anixops_json_find_path_value(body, rule->body_pattern, &value_start, &value_end);
	if (found < 0) {
		*out_path_supported = 0;
		return anixops_copy_text_checked(out, out_cap, body);
	}
	if (!found) {
		return anixops_copy_text_checked(out, out_cap, body);
	}

	out[0] = '\0';
	if (anixops_append_range(out, out_cap, &pos, body, (size_t)(value_start - body)) != ANIXOPS_OK) {
		truncated = 1;
	}
	if (anixops_append_json_replacement(out, out_cap, &pos, rule->replacement) != ANIXOPS_OK) {
		truncated = 1;
	}
	if (anixops_append_range(out, out_cap, &pos, value_end, strlen(value_end)) != ANIXOPS_OK) {
		truncated = 1;
	}
	if (out_cap > 0) {
		out[pos < out_cap ? pos : out_cap - 1] = '\0';
	}
	*out_replaced = 1;
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_apply_regex_replacement(
	const char *input,
	const regex_t *regex,
	int regex_ready,
	const char *replacement,
	char *out,
	size_t out_cap,
	int *out_replaced)
{
	const char *cursor = input;
	size_t pos = 0;
	int truncated = 0;
	int replaced = 0;
	regmatch_t matches[10];

	if (input == NULL || regex == NULL || out == NULL || out_cap == 0 || out_replaced == NULL || !regex_ready) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	out[0] = '\0';

	while (regexec(regex, cursor, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
		size_t prefix_len;
		size_t advance;
		if (matches[0].rm_so < 0 || matches[0].rm_eo < matches[0].rm_so) {
			break;
		}
		prefix_len = (size_t)matches[0].rm_so;
		advance = (size_t)matches[0].rm_eo;
		if (anixops_append_range(out, out_cap, &pos, cursor, prefix_len) != ANIXOPS_OK) {
			truncated = 1;
		}
		if (anixops_append_expanded_replacement(
				cursor,
				replacement,
				matches,
				sizeof(matches) / sizeof(matches[0]),
				out,
				out_cap,
				&pos) != ANIXOPS_OK) {
			truncated = 1;
		}
		replaced = 1;
		if (advance == 0) {
			if (cursor[0] == '\0') {
				break;
			}
			if (anixops_append_char(out, out_cap, &pos, cursor[0]) != ANIXOPS_OK) {
				truncated = 1;
			}
			cursor++;
			continue;
		}
		cursor += advance;
	}
	if (anixops_append_range(out, out_cap, &pos, cursor, strlen(cursor)) != ANIXOPS_OK) {
		truncated = 1;
	}
	if (out_cap > 0) {
		out[pos < out_cap ? pos : out_cap - 1] = '\0';
	}
	*out_replaced = replaced;
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_append_expanded_replacement(
	const char *input,
	const char *replacement,
	const regmatch_t *matches,
	size_t match_count,
	char *out,
	size_t out_cap,
	size_t *pos)
{
	const char *p = replacement == NULL ? "" : replacement;
	int truncated = 0;
	if (input == NULL || out == NULL || out_cap == 0 || pos == NULL) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	while (*p != '\0') {
		if ((*p == '$' || *p == '\\') && isdigit((unsigned char)p[1])) {
			size_t idx = (size_t)(p[1] - '0');
			if (idx < match_count && matches[idx].rm_so >= 0 && matches[idx].rm_eo >= matches[idx].rm_so) {
				size_t start = (size_t)matches[idx].rm_so;
				size_t len = (size_t)(matches[idx].rm_eo - matches[idx].rm_so);
				if (anixops_append_range(out, out_cap, pos, input + start, len) != ANIXOPS_OK) {
					truncated = 1;
				}
			}
			p += 2;
			continue;
		}
		if (*p == '\\' && p[1] == '/') {
			if (anixops_append_char(out, out_cap, pos, '/') != ANIXOPS_OK) {
				truncated = 1;
			}
			p += 2;
			continue;
		}
		if (anixops_append_char(out, out_cap, pos, *p) != ANIXOPS_OK) {
			truncated = 1;
		}
		p++;
	}
	return truncated ? ANIXOPS_ERR_CAPACITY : ANIXOPS_OK;
}

static int anixops_append_json_replacement(char *out, size_t out_cap, size_t *pos, const char *replacement)
{
	const char *raw_start;
	const char *raw_end;
	if (anixops_json_replacement_is_raw_value(replacement, &raw_start, &raw_end)) {
		return anixops_append_range(out, out_cap, pos, raw_start, (size_t)(raw_end - raw_start));
	}
	return anixops_append_json_string(out, out_cap, pos, replacement == NULL ? "" : replacement);
}

static int anixops_append_json_string(char *out, size_t out_cap, size_t *pos, const char *value)
{
	const unsigned char *p = (const unsigned char *)(value == NULL ? "" : value);
	int rc = ANIXOPS_OK;
	if (anixops_append_char(out, out_cap, pos, '"') != ANIXOPS_OK) {
		rc = ANIXOPS_ERR_CAPACITY;
	}
	while (*p != '\0') {
		char escaped[7];
		const char *replacement = NULL;
		size_t replacement_len = 0;
		switch (*p) {
		case '"':
			replacement = "\\\"";
			replacement_len = 2;
			break;
		case '\\':
			replacement = "\\\\";
			replacement_len = 2;
			break;
		case '\b':
			replacement = "\\b";
			replacement_len = 2;
			break;
		case '\f':
			replacement = "\\f";
			replacement_len = 2;
			break;
		case '\n':
			replacement = "\\n";
			replacement_len = 2;
			break;
		case '\r':
			replacement = "\\r";
			replacement_len = 2;
			break;
		case '\t':
			replacement = "\\t";
			replacement_len = 2;
			break;
		default:
			if (*p < 0x20) {
				snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int)*p);
				replacement = escaped;
				replacement_len = strlen(escaped);
			}
			break;
		}
		if (replacement != NULL) {
			if (anixops_append_range(out, out_cap, pos, replacement, replacement_len) != ANIXOPS_OK) {
				rc = ANIXOPS_ERR_CAPACITY;
			}
		}
		else if (anixops_append_char(out, out_cap, pos, (char)*p) != ANIXOPS_OK) {
			rc = ANIXOPS_ERR_CAPACITY;
		}
		p++;
	}
	if (anixops_append_char(out, out_cap, pos, '"') != ANIXOPS_OK) {
		rc = ANIXOPS_ERR_CAPACITY;
	}
	return rc;
}

static int anixops_json_find_path_value(
	const char *body,
	const char *path,
	const char **out_value_start,
	const char **out_value_end)
{
	const char *cursor;
	const char *value_start;
	const char *value_end;
	char bracket_key[ANIXOPS_VALUE_CAP];
	int parsed_segment = 0;
	if (body == NULL || path == NULL || out_value_start == NULL || out_value_end == NULL) {
		return 0;
	}
	cursor = path;
	if (*cursor == '$') {
		cursor++;
	}
	value_start = body;
	value_end = body + strlen(body);
	while (*cursor != '\0') {
		const char *key;
		size_t key_len;
		int found;

		if (*cursor == '.') {
			cursor++;
			if (!anixops_json_parse_path_segment(&cursor, &key, &key_len)) {
				return -1;
			}
			found = anixops_json_find_member_value(value_start, value_end, key, key_len, &value_start, &value_end);
			if (!found) {
				return 0;
			}
			parsed_segment = 1;
			continue;
		}
		if (*cursor == '[') {
			if (cursor[1] == '\'' || cursor[1] == '"') {
				if (!anixops_json_parse_bracket_key(&cursor, bracket_key, sizeof(bracket_key), &key, &key_len)) {
					return -1;
				}
				found = anixops_json_find_member_value(value_start, value_end, key, key_len, &value_start, &value_end);
				if (!found) {
					return 0;
				}
				parsed_segment = 1;
				continue;
			}
			size_t index;
			if (!anixops_json_parse_array_index(&cursor, &index)) {
				return -1;
			}
			found = anixops_json_find_array_element_value(value_start, value_end, index, &value_start, &value_end);
			if (!found) {
				return 0;
			}
			parsed_segment = 1;
			continue;
		}
		return -1;
	}
	if (!parsed_segment) {
		return -1;
	}
	*out_value_start = value_start;
	*out_value_end = value_end;
	return 1;
}

static int anixops_json_find_member_value(
	const char *object_start,
	const char *object_end,
	const char *key,
	size_t key_len,
	const char **out_value_start,
	const char **out_value_end)
{
	const char *cursor;
	if (object_start == NULL || object_end == NULL || key == NULL || out_value_start == NULL || out_value_end == NULL) {
		return 0;
	}
	cursor = anixops_json_skip_ws(object_start, object_end);
	if (cursor >= object_end || *cursor != '{') {
		return 0;
	}
	cursor++;
	cursor = anixops_json_skip_ws(cursor, object_end);
	if (cursor < object_end && *cursor == '}') {
		return 0;
	}
	while (cursor < object_end) {
		const char *key_end;
		const char *value_start;
		const char *value_end;
		int key_matches;

		if (*cursor != '"') {
			return 0;
		}
		key_end = anixops_json_skip_string(cursor, object_end);
		if (key_end == NULL) {
			return 0;
		}
		key_matches = anixops_json_key_matches(cursor, key_end, key, key_len);
		cursor = anixops_json_skip_ws(key_end, object_end);
		if (cursor >= object_end || *cursor != ':') {
			return 0;
		}
		cursor = anixops_json_skip_ws(cursor + 1, object_end);
		value_start = cursor;
		value_end = anixops_json_skip_value(value_start, object_end);
		if (value_end == NULL) {
			return 0;
		}
		cursor = anixops_json_skip_ws(value_end, object_end);
		if (cursor >= object_end || (*cursor != ',' && *cursor != '}')) {
			return 0;
		}
		if (key_matches) {
			*out_value_start = value_start;
			*out_value_end = value_end;
			return 1;
		}
		if (*cursor == ',') {
			cursor = anixops_json_skip_ws(cursor + 1, object_end);
			continue;
		}
		if (*cursor == '}') {
			return 0;
		}
		return 0;
	}
	return 0;
}

static int anixops_json_find_array_element_value(
	const char *array_start,
	const char *array_end,
	size_t index,
	const char **out_value_start,
	const char **out_value_end)
{
	const char *cursor;
	size_t current = 0;
	if (array_start == NULL || array_end == NULL || out_value_start == NULL || out_value_end == NULL) {
		return 0;
	}
	cursor = anixops_json_skip_ws(array_start, array_end);
	if (cursor >= array_end || *cursor != '[') {
		return 0;
	}
	cursor = anixops_json_skip_ws(cursor + 1, array_end);
	if (cursor < array_end && *cursor == ']') {
		return 0;
	}
	while (cursor < array_end) {
		const char *value_start = cursor;
		const char *value_end = anixops_json_skip_value(value_start, array_end);
		const char *after;
		if (value_end == NULL) {
			return 0;
		}
		after = anixops_json_skip_ws(value_end, array_end);
		if (after >= array_end || (*after != ',' && *after != ']')) {
			return 0;
		}
		if (current == index) {
			*out_value_start = value_start;
			*out_value_end = value_end;
			return 1;
		}
		if (*after == ',') {
			current++;
			cursor = anixops_json_skip_ws(after + 1, array_end);
			continue;
		}
		return 0;
	}
	return 0;
}

static int anixops_json_key_matches(const char *string_start, const char *string_end, const char *key, size_t key_len)
{
	const char *cursor;
	const char *content_end;
	size_t pos = 0;
	if (string_start == NULL || string_end == NULL || key == NULL || string_end <= string_start + 1 ||
		*string_start != '"') {
		return 0;
	}
	cursor = string_start + 1;
	content_end = string_end - 1;
	while (cursor < content_end) {
		unsigned char ch = (unsigned char)*cursor++;
		if (ch == '\\') {
			char decoded[4];
			size_t decoded_len = 0;
			size_t i;
			if (cursor >= content_end) {
				return 0;
			}
			if (!anixops_json_decode_escape(&cursor, content_end, decoded, sizeof(decoded), &decoded_len)) {
				return 0;
			}
			if (pos + decoded_len > key_len) {
				return 0;
			}
			for (i = 0; i < decoded_len; i++) {
				if (key[pos + i] != decoded[i]) {
					return 0;
				}
			}
			pos += decoded_len;
			continue;
		}
		if (pos >= key_len || key[pos] != (char)ch) {
			return 0;
		}
		pos++;
	}
	return pos == key_len;
}

static int anixops_json_parse_path_segment(const char **cursor, const char **out_key, size_t *out_key_len)
{
	const char *start;
	const char *p;
	if (cursor == NULL || *cursor == NULL || out_key == NULL || out_key_len == NULL) {
		return 0;
	}
	start = *cursor;
	p = start;
	while (*p != '\0' && *p != '.' && *p != '[') {
		unsigned char ch = (unsigned char)*p;
		if (!(isalnum(ch) || ch == '_' || ch == '-')) {
			return 0;
		}
		p++;
	}
	if (p == start) {
		return 0;
	}
	*out_key = start;
	*out_key_len = (size_t)(p - start);
	*cursor = p;
	return 1;
}

static int anixops_json_parse_bracket_key(
	const char **cursor,
	char *out,
	size_t out_cap,
	const char **out_key,
	size_t *out_key_len)
{
	const char *p;
	const char *end;
	size_t pos = 0;
	char quote;
	if (cursor == NULL || *cursor == NULL || out == NULL || out_cap == 0 || out_key == NULL || out_key_len == NULL ||
		**cursor != '[') {
		return 0;
	}
	end = *cursor + strlen(*cursor);
	p = *cursor + 1;
	if (*p != '\'' && *p != '"') {
		return 0;
	}
	quote = *p;
	p++;
	while (*p != '\0' && *p != quote) {
		char ch = *p++;
		if (ch == '\\') {
			char decoded[4];
			size_t decoded_len = 0;
			size_t i;
			if (!anixops_json_decode_escape(&p, end, decoded, sizeof(decoded), &decoded_len)) {
				return 0;
			}
			if (pos + decoded_len >= out_cap) {
				return 0;
			}
			for (i = 0; i < decoded_len; i++) {
				out[pos++] = decoded[i];
			}
			continue;
		}
		if (pos + 1 >= out_cap) {
			return 0;
		}
		out[pos++] = ch;
	}
	if (*p != quote || p[1] != ']') {
		return 0;
	}
	out[pos] = '\0';
	*out_key = out;
	*out_key_len = pos;
	*cursor = p + 2;
	return 1;
}

static int anixops_json_decode_escape(const char **cursor, const char *end, char *out, size_t out_cap, size_t *out_len)
{
	const char *p;
	unsigned char ch;
	unsigned long codepoint;
	size_t pos = 0;

	if (cursor == NULL || *cursor == NULL || end == NULL || out == NULL || out_cap == 0 || out_len == NULL) {
		return 0;
	}
	p = *cursor;
	if (p >= end) {
		return 0;
	}
	ch = (unsigned char)*p++;
	switch (ch) {
	case '\'':
	case '"':
	case '\\':
	case '/':
		out[pos++] = (char)ch;
		break;
	case 'b':
		out[pos++] = '\b';
		break;
	case 'f':
		out[pos++] = '\f';
		break;
	case 'n':
		out[pos++] = '\n';
		break;
	case 'r':
		out[pos++] = '\r';
		break;
	case 't':
		out[pos++] = '\t';
		break;
	case 'u':
		if (!anixops_json_read_hex4(&p, end, &codepoint)) {
			return 0;
		}
		if (codepoint >= 0xD800UL && codepoint <= 0xDBFFUL) {
			unsigned long low;
			if ((size_t)(end - p) < 2 || p[0] != '\\' || p[1] != 'u') {
				return 0;
			}
			p += 2;
			if (!anixops_json_read_hex4(&p, end, &low) || low < 0xDC00UL || low > 0xDFFFUL) {
				return 0;
			}
			codepoint = 0x10000UL + ((codepoint - 0xD800UL) << 10) + (low - 0xDC00UL);
		}
		else if (codepoint >= 0xDC00UL && codepoint <= 0xDFFFUL) {
			return 0;
		}
		if (!anixops_json_append_utf8(out, out_cap, &pos, codepoint)) {
			return 0;
		}
		break;
	default:
		return 0;
	}
	*cursor = p;
	*out_len = pos;
	return 1;
}

static int anixops_json_read_hex4(const char **cursor, const char *end, unsigned long *out_value)
{
	const char *p;
	unsigned long value = 0;
	size_t i;

	if (cursor == NULL || *cursor == NULL || end == NULL || out_value == NULL) {
		return 0;
	}
	p = *cursor;
	for (i = 0; i < 4; i++) {
		int digit;
		if (p >= end) {
			return 0;
		}
		digit = anixops_hex_digit((unsigned char)*p);
		if (digit < 0) {
			return 0;
		}
		value = (value << 4) | (unsigned long)digit;
		p++;
	}
	*cursor = p;
	*out_value = value;
	return 1;
}

static int anixops_json_append_utf8(char *out, size_t out_cap, size_t *pos, unsigned long codepoint)
{
	size_t p;
	if (out == NULL || pos == NULL || codepoint > 0x10FFFFUL) {
		return 0;
	}
	p = *pos;
	if (codepoint <= 0x7FUL) {
		if (p + 1 > out_cap) {
			return 0;
		}
		out[p++] = (char)codepoint;
	}
	else if (codepoint <= 0x7FFUL) {
		if (p + 2 > out_cap) {
			return 0;
		}
		out[p++] = (char)(0xC0U | (codepoint >> 6));
		out[p++] = (char)(0x80U | (codepoint & 0x3FUL));
	}
	else if (codepoint <= 0xFFFFUL) {
		if (codepoint >= 0xD800UL && codepoint <= 0xDFFFUL) {
			return 0;
		}
		if (p + 3 > out_cap) {
			return 0;
		}
		out[p++] = (char)(0xE0U | (codepoint >> 12));
		out[p++] = (char)(0x80U | ((codepoint >> 6) & 0x3FUL));
		out[p++] = (char)(0x80U | (codepoint & 0x3FUL));
	}
	else {
		if (p + 4 > out_cap) {
			return 0;
		}
		out[p++] = (char)(0xF0U | (codepoint >> 18));
		out[p++] = (char)(0x80U | ((codepoint >> 12) & 0x3FUL));
		out[p++] = (char)(0x80U | ((codepoint >> 6) & 0x3FUL));
		out[p++] = (char)(0x80U | (codepoint & 0x3FUL));
	}
	*pos = p;
	return 1;
}

static int anixops_hex_digit(unsigned char ch)
{
	if (ch >= '0' && ch <= '9') {
		return (int)(ch - '0');
	}
	if (ch >= 'a' && ch <= 'f') {
		return (int)(ch - 'a' + 10);
	}
	if (ch >= 'A' && ch <= 'F') {
		return (int)(ch - 'A' + 10);
	}
	return -1;
}

static int anixops_json_parse_array_index(const char **cursor, size_t *out_index)
{
	const char *p;
	size_t value = 0;
	if (cursor == NULL || *cursor == NULL || out_index == NULL || **cursor != '[') {
		return 0;
	}
	p = *cursor + 1;
	if (!isdigit((unsigned char)*p)) {
		return 0;
	}
	while (isdigit((unsigned char)*p)) {
		size_t digit = (size_t)(*p - '0');
		if (value > (((size_t)-1) - digit) / 10) {
			return 0;
		}
		value = value * 10 + digit;
		p++;
	}
	if (*p != ']') {
		return 0;
	}
	*out_index = value;
	*cursor = p + 1;
	return 1;
}

static int anixops_json_replacement_is_raw_value(const char *value, const char **out_start, const char **out_end)
{
	const char *start;
	const char *end;
	const char *value_end;
	if (value == NULL) {
		return 0;
	}
	end = value + strlen(value);
	start = anixops_json_skip_ws(value, end);
	if (start == end) {
		return 0;
	}
	value_end = anixops_json_skip_value(start, end);
	if (value_end == NULL) {
		return 0;
	}
	if (anixops_json_skip_ws(value_end, end) != end) {
		return 0;
	}
	*out_start = start;
	*out_end = value_end;
	return 1;
}

static const char *anixops_json_skip_ws(const char *cursor, const char *end)
{
	while (cursor < end && isspace((unsigned char)*cursor)) {
		cursor++;
	}
	return cursor;
}

static const char *anixops_json_skip_string(const char *cursor, const char *end)
{
	if (cursor >= end || *cursor != '"') {
		return NULL;
	}
	cursor++;
	while (cursor < end) {
		unsigned char ch = (unsigned char)*cursor++;
		if (ch == '"') {
			return cursor;
		}
		if (ch == '\\') {
			unsigned char escaped;
			if (cursor >= end) {
				return NULL;
			}
			escaped = (unsigned char)*cursor++;
			if (escaped == 'u') {
				size_t i;
				for (i = 0; i < 4; i++) {
					if (cursor >= end || !isxdigit((unsigned char)*cursor)) {
						return NULL;
					}
					cursor++;
				}
			}
			else if (!(escaped == '"' || escaped == '\\' || escaped == '/' || escaped == 'b' || escaped == 'f' ||
					   escaped == 'n' || escaped == 'r' || escaped == 't')) {
				return NULL;
			}
		}
		else if (ch < 0x20) {
			return NULL;
		}
	}
	return NULL;
}

static const char *anixops_json_skip_number(const char *cursor, const char *end)
{
	if (cursor < end && *cursor == '-') {
		cursor++;
	}
	if (cursor >= end) {
		return NULL;
	}
	if (*cursor == '0') {
		cursor++;
	}
	else if (*cursor >= '1' && *cursor <= '9') {
		while (cursor < end && isdigit((unsigned char)*cursor)) {
			cursor++;
		}
	}
	else {
		return NULL;
	}
	if (cursor < end && *cursor == '.') {
		cursor++;
		if (cursor >= end || !isdigit((unsigned char)*cursor)) {
			return NULL;
		}
		while (cursor < end && isdigit((unsigned char)*cursor)) {
			cursor++;
		}
	}
	if (cursor < end && (*cursor == 'e' || *cursor == 'E')) {
		cursor++;
		if (cursor < end && (*cursor == '+' || *cursor == '-')) {
			cursor++;
		}
		if (cursor >= end || !isdigit((unsigned char)*cursor)) {
			return NULL;
		}
		while (cursor < end && isdigit((unsigned char)*cursor)) {
			cursor++;
		}
	}
	return cursor;
}

static const char *anixops_json_skip_value(const char *cursor, const char *end)
{
	cursor = anixops_json_skip_ws(cursor, end);
	if (cursor >= end) {
		return NULL;
	}
	if (*cursor == '"') {
		return anixops_json_skip_string(cursor, end);
	}
	if (*cursor == '{') {
		cursor++;
		cursor = anixops_json_skip_ws(cursor, end);
		if (cursor < end && *cursor == '}') {
			return cursor + 1;
		}
		while (cursor < end) {
			if (*cursor != '"') {
				return NULL;
			}
			cursor = anixops_json_skip_string(cursor, end);
			if (cursor == NULL) {
				return NULL;
			}
			cursor = anixops_json_skip_ws(cursor, end);
			if (cursor >= end || *cursor != ':') {
				return NULL;
			}
			cursor = anixops_json_skip_value(cursor + 1, end);
			if (cursor == NULL) {
				return NULL;
			}
			cursor = anixops_json_skip_ws(cursor, end);
			if (cursor < end && *cursor == ',') {
				cursor = anixops_json_skip_ws(cursor + 1, end);
				continue;
			}
			if (cursor < end && *cursor == '}') {
				return cursor + 1;
			}
			return NULL;
		}
		return NULL;
	}
	if (*cursor == '[') {
		cursor++;
		cursor = anixops_json_skip_ws(cursor, end);
		if (cursor < end && *cursor == ']') {
			return cursor + 1;
		}
		while (cursor < end) {
			cursor = anixops_json_skip_value(cursor, end);
			if (cursor == NULL) {
				return NULL;
			}
			cursor = anixops_json_skip_ws(cursor, end);
			if (cursor < end && *cursor == ',') {
				cursor = anixops_json_skip_ws(cursor + 1, end);
				continue;
			}
			if (cursor < end && *cursor == ']') {
				return cursor + 1;
			}
			return NULL;
		}
		return NULL;
	}
	if (*cursor == '-' || isdigit((unsigned char)*cursor)) {
		return anixops_json_skip_number(cursor, end);
	}
	if ((size_t)(end - cursor) >= 4 && strncmp(cursor, "true", 4) == 0) {
		return cursor + 4;
	}
	if ((size_t)(end - cursor) >= 5 && strncmp(cursor, "false", 5) == 0) {
		return cursor + 5;
	}
	if ((size_t)(end - cursor) >= 4 && strncmp(cursor, "null", 4) == 0) {
		return cursor + 4;
	}
	return NULL;
}

static int anixops_append_range(char *out, size_t out_cap, size_t *pos, const char *start, size_t len)
{
	size_t i;
	int rc = ANIXOPS_OK;
	for (i = 0; i < len; i++) {
		if (anixops_append_char(out, out_cap, pos, start[i]) != ANIXOPS_OK) {
			rc = ANIXOPS_ERR_CAPACITY;
		}
	}
	return rc;
}

static int anixops_append_char(char *out, size_t out_cap, size_t *pos, char value)
{
	if (*pos + 1 >= out_cap) {
		return ANIXOPS_ERR_CAPACITY;
	}
	out[*pos] = value;
	(*pos)++;
	out[*pos] = '\0';
	return ANIXOPS_OK;
}

static int anixops_wildcard_match(const char *pattern, const char *text)
{
	const char *star = NULL;
	const char *retry = NULL;
	while (*text != '\0') {
		if (*pattern == '*') {
			star = pattern++;
			retry = text;
		}
		else if (*pattern == *text) {
			pattern++;
			text++;
		}
		else if (star != NULL) {
			pattern = star + 1;
			text = ++retry;
		}
		else {
			return 0;
		}
	}
	while (*pattern == '*') {
		pattern++;
	}
	return *pattern == '\0';
}

static void anixops_normalize_host(const char *input, char *out, size_t out_cap)
{
	size_t len;
	const char *start = input;
	const char *end;
	size_t colon_count = 0;
	const char *last_colon = NULL;
	const char *p;

	if (out_cap == 0) {
		return;
	}
	out[0] = '\0';
	if (input == NULL) {
		return;
	}
	while (*start != '\0' && isspace((unsigned char)*start)) {
		start++;
	}
	end = start + strlen(start);
	while (end > start && isspace((unsigned char)*(end - 1))) {
		end--;
	}
	if (end > start && *start == '[') {
		const char *close = memchr(start, ']', (size_t)(end - start));
		if (close != NULL) {
			start++;
			end = close;
		}
	}
	for (p = start; p < end; p++) {
		if (*p == ':') {
			colon_count++;
			last_colon = p;
		}
	}
	if (colon_count == 1 && last_colon != NULL) {
		const char *q = last_colon + 1;
		int all_digits = *q != '\0';
		while (q < end) {
			if (!isdigit((unsigned char)*q)) {
				all_digits = 0;
				break;
			}
			q++;
		}
		if (all_digits) {
			end = last_colon;
		}
	}
	len = (size_t)(end - start);
	if (len >= out_cap) {
		len = out_cap - 1;
	}
	memcpy(out, start, len);
	out[len] = '\0';
	anixops_lower_inplace(out);
}

static int anixops_host_pattern_matches(const char *pattern, const char *host)
{
	if (pattern == NULL || host == NULL || pattern[0] == '\0' || host[0] == '\0') {
		return 0;
	}
	if (strcmp(pattern, "*") == 0) {
		return 1;
	}
	if (anixops_starts_with_ci(pattern, "*.") && strcmp(pattern + 2, host) == 0) {
		return 1;
	}
	return anixops_wildcard_match(pattern, host);
}

static void anixops_set_mitm_result(
	anixops_mitm_decision_t *out,
	anixops_mitm_decision_type_t decision,
	anixops_mitm_reason_t reason,
	const char *pattern,
	const char *message)
{
	memset(out, 0, sizeof(*out));
	out->decision = decision;
	out->reason = reason;
	anixops_copy_text(out->matched_pattern, sizeof(out->matched_pattern), pattern == NULL ? "" : pattern);
	anixops_copy_text(out->message, sizeof(out->message), message == NULL ? "" : message);
}

static void anixops_set_rewrite_none(anixops_rewrite_result_t *out)
{
	memset(out, 0, sizeof(*out));
	out->action = ANIXOPS_REWRITE_NONE;
	out->status_code = 0;
	out->rule_index = -1;
	anixops_copy_text(out->message, sizeof(out->message), "no rewrite matched");
}

static void anixops_set_header_rewrite_none(anixops_header_rewrite_result_t *out)
{
	memset(out, 0, sizeof(*out));
	out->action = ANIXOPS_REWRITE_NONE;
	out->phase = ANIXOPS_PHASE_REQUEST;
	out->rule_index = -1;
	anixops_copy_text(out->message, sizeof(out->message), "no header rewrite matched");
}

static void anixops_set_script_none(anixops_script_result_t *out)
{
	memset(out, 0, sizeof(*out));
	out->kind = ANIXOPS_SCRIPT_NONE;
	out->phase = ANIXOPS_PHASE_REQUEST;
	out->requires_body = 0;
	out->rule_index = -1;
	anixops_copy_text(out->message, sizeof(out->message), "no script matched");
}

static void anixops_set_diagnostic(anixops_engine_t *engine, int status, size_t line, const char *message)
{
	if (engine == NULL) {
		return;
	}
	engine->last_status = status;
	engine->last_error_line = line;
	anixops_copy_text(
		engine->last_error_message,
		sizeof(engine->last_error_message),
		message == NULL ? anixops_status_message(status) : message);
}

static void anixops_clear_diagnostic(anixops_engine_t *engine)
{
	anixops_set_diagnostic(engine, ANIXOPS_OK, 0, anixops_status_message(ANIXOPS_OK));
}

static int anixops_config_line_error(anixops_engine_t *engine, int status, size_t line_no, char *line)
{
	char message[ANIXOPS_MESSAGE_CAP];
	(void)line;
	anixops_copy_text(
		message,
		sizeof(message),
		engine != NULL && engine->last_error_message[0] != '\0' ?
			engine->last_error_message :
			anixops_status_message(status));
	free(line);
	anixops_set_diagnostic(engine, status, line_no, message);
	return status;
}

static void anixops_copy_text(char *dst, size_t cap, const char *src)
{
	if (dst == NULL || cap == 0) {
		return;
	}
	if (src == NULL) {
		dst[0] = '\0';
		return;
	}
	snprintf(dst, cap, "%s", src);
}

static int anixops_copy_text_checked(char *dst, size_t cap, const char *src)
{
	size_t len;
	if (dst == NULL || cap == 0) {
		return ANIXOPS_ERR_INVALID_ARGUMENT;
	}
	if (src == NULL) {
		dst[0] = '\0';
		return ANIXOPS_OK;
	}
	len = strlen(src);
	if (len >= cap) {
		memcpy(dst, src, cap - 1);
		dst[cap - 1] = '\0';
		return ANIXOPS_ERR_CAPACITY;
	}
	memcpy(dst, src, len + 1);
	return ANIXOPS_OK;
}
