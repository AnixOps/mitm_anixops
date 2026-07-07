#define _POSIX_C_SOURCE 200809L

#include "mitm_anixops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCRIPT_MAP_CAP 8
#define CORPUS_FIXTURE_CAP 32

typedef struct corpus_fixture {
	char name[256];
	char platform[64];
	char path[1024];
	char source[256];
	char sha256[65];
	int expected_status;
	size_t expected_mitm;
	size_t expected_rewrite;
	size_t expected_script;
	size_t expected_argument;
} corpus_fixture_t;

static char *read_file(const char *path)
{
	FILE *fp;
	long size;
	char *data;
	size_t read_len;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return NULL;
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	size = ftell(fp);
	if (size < 0) {
		fclose(fp);
		return NULL;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}
	data = (char *)malloc((size_t)size + 1);
	if (data == NULL) {
		fclose(fp);
		return NULL;
	}
	read_len = fread(data, 1, (size_t)size, fp);
	fclose(fp);
	if (read_len != (size_t)size) {
		free(data);
		return NULL;
	}
	data[read_len] = '\0';
	return data;
}

static int copy_text(char *out, size_t out_cap, const char *value)
{
	size_t len;
	if (out == NULL || out_cap == 0 || value == NULL) {
		return 0;
	}
	len = strlen(value);
	if (len >= out_cap) {
		return 0;
	}
	memcpy(out, value, len + 1);
	return 1;
}

static int json_skip_ws(const char **cursor, const char *end)
{
	while (*cursor < end && (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' || **cursor == '\r')) {
		(*cursor)++;
	}
	return *cursor < end;
}

static const char *json_object_end(const char *object_start)
{
	const char *p = object_start;
	int depth = 0;
	int in_string = 0;
	int escaped = 0;
	while (*p != '\0') {
		if (in_string) {
			if (escaped) {
				escaped = 0;
			}
			else if (*p == '\\') {
				escaped = 1;
			}
			else if (*p == '"') {
				in_string = 0;
			}
		}
		else if (*p == '"') {
			in_string = 1;
		}
		else if (*p == '{') {
			depth++;
		}
		else if (*p == '}') {
			depth--;
			if (depth == 0) {
				return p;
			}
		}
		p++;
	}
	return NULL;
}

static int json_parse_string_value(const char **cursor, const char *end, char *out, size_t out_cap)
{
	size_t pos = 0;
	const char *p = *cursor;
	if (p >= end || *p != '"') {
		return 0;
	}
	p++;
	while (p < end && *p != '"') {
		char ch = *p++;
		if (ch == '\\') {
			if (p >= end) {
				return 0;
			}
			ch = *p++;
			switch (ch) {
			case '"':
			case '\\':
			case '/':
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			default:
				return 0;
			}
		}
		if (pos + 1 >= out_cap) {
			return 0;
		}
		out[pos++] = ch;
	}
	if (p >= end || *p != '"') {
		return 0;
	}
	out[pos] = '\0';
	*cursor = p + 1;
	return 1;
}

static const char *json_find_field(const char *object_start, const char *object_end, const char *field)
{
	char needle[128];
	const char *p;
	if (snprintf(needle, sizeof(needle), "\"%s\"", field) >= (int)sizeof(needle)) {
		return NULL;
	}
	p = object_start;
	while (p < object_end) {
		const char *found = strstr(p, needle);
		if (found == NULL || found >= object_end) {
			return NULL;
		}
		p = found + strlen(needle);
		while (p < object_end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
			p++;
		}
		if (p < object_end && *p == ':') {
			p++;
			while (p < object_end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
				p++;
			}
			return p;
		}
	}
	return NULL;
}

static int json_object_string_field(
	const char *object_start,
	const char *object_end,
	const char *field,
	char *out,
	size_t out_cap)
{
	const char *value = json_find_field(object_start, object_end, field);
	if (value == NULL) {
		return 0;
	}
	return json_parse_string_value(&value, object_end, out, out_cap);
}

static int json_object_size_field(const char *object_start, const char *object_end, const char *field, size_t *out)
{
	const char *value = json_find_field(object_start, object_end, field);
	char *number_end;
	unsigned long parsed;
	if (value == NULL || value >= object_end || out == NULL) {
		return 0;
	}
	parsed = strtoul(value, &number_end, 10);
	if (number_end == value || number_end > object_end) {
		return 0;
	}
	*out = (size_t)parsed;
	return 1;
}

static int json_object_int_field(const char *object_start, const char *object_end, const char *field, int *out)
{
	size_t parsed;
	if (!json_object_size_field(object_start, object_end, field, &parsed) || out == NULL) {
		return 0;
	}
	*out = (int)parsed;
	return 1;
}

static int parse_corpus_manifest(const char *text, corpus_fixture_t *fixtures, size_t *out_count)
{
	const char *fixtures_key;
	const char *cursor;
	size_t count = 0;
	if (text == NULL || fixtures == NULL || out_count == NULL) {
		return 0;
	}
	fixtures_key = strstr(text, "\"fixtures\"");
	if (fixtures_key == NULL) {
		return 0;
	}
	cursor = strchr(fixtures_key, '[');
	if (cursor == NULL) {
		return 0;
	}
	cursor++;
	for (;;) {
		const char *object_start;
		const char *object_end;
		corpus_fixture_t fixture;
		if (!json_skip_ws(&cursor, text + strlen(text))) {
			return 0;
		}
		if (*cursor == ']') {
			break;
		}
		if (*cursor != '{') {
			return 0;
		}
		if (count >= CORPUS_FIXTURE_CAP) {
			return 0;
		}
		object_start = cursor;
		object_end = json_object_end(object_start);
		if (object_end == NULL) {
			return 0;
		}
		memset(&fixture, 0, sizeof(fixture));
		if (!json_object_string_field(object_start, object_end, "name", fixture.name, sizeof(fixture.name)) ||
			!json_object_string_field(object_start, object_end, "platform", fixture.platform, sizeof(fixture.platform)) ||
			!json_object_string_field(object_start, object_end, "path", fixture.path, sizeof(fixture.path)) ||
			!json_object_string_field(object_start, object_end, "source", fixture.source, sizeof(fixture.source)) ||
			!json_object_string_field(object_start, object_end, "sha256", fixture.sha256, sizeof(fixture.sha256)) ||
			!json_object_int_field(object_start, object_end, "expectedStatus", &fixture.expected_status) ||
			!json_object_size_field(object_start, object_end, "expectedMitm", &fixture.expected_mitm) ||
			!json_object_size_field(object_start, object_end, "expectedRewrite", &fixture.expected_rewrite) ||
			!json_object_size_field(object_start, object_end, "expectedScript", &fixture.expected_script) ||
			!json_object_size_field(object_start, object_end, "expectedArgument", &fixture.expected_argument)) {
			return 0;
		}
		fixtures[count++] = fixture;
		cursor = object_end + 1;
		if (!json_skip_ws(&cursor, text + strlen(text))) {
			return 0;
		}
		if (*cursor == ',') {
			cursor++;
		}
		else if (*cursor == ']') {
			break;
		}
		else {
			return 0;
		}
	}
	*out_count = count;
	return 1;
}

static int dirname_for_path(const char *path, char *out, size_t out_cap)
{
	const char *slash;
	size_t len;
	if (path == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	slash = strrchr(path, '/');
	if (slash == NULL) {
		return copy_text(out, out_cap, ".");
	}
	len = (size_t)(slash - path);
	if (len == 0) {
		len = 1;
	}
	if (len >= out_cap) {
		return 0;
	}
	memcpy(out, path, len);
	out[len] = '\0';
	return 1;
}

static int resolve_manifest_path(const char *manifest_path, const char *fixture_path, char *out, size_t out_cap)
{
	char dir[1024];
	if (manifest_path == NULL || fixture_path == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	if (fixture_path[0] == '/') {
		return copy_text(out, out_cap, fixture_path);
	}
	if (!dirname_for_path(manifest_path, dir, sizeof(dir))) {
		return 0;
	}
	if (snprintf(out, out_cap, "%s/%s", dir, fixture_path) >= (int)out_cap) {
		return 0;
	}
	return 1;
}

static void json_string(const char *value)
{
	const unsigned char *p = (const unsigned char *)(value == NULL ? "" : value);
	putchar('"');
	while (*p != '\0') {
		switch (*p) {
		case '\\':
			fputs("\\\\", stdout);
			break;
		case '"':
			fputs("\\\"", stdout);
			break;
		case '\n':
			fputs("\\n", stdout);
			break;
		case '\r':
			fputs("\\r", stdout);
			break;
		case '\t':
			fputs("\\t", stdout);
			break;
		default:
			if (*p < 0x20) {
				printf("\\u%04x", (unsigned int)*p);
			}
			else {
				putchar((int)*p);
			}
			break;
		}
		p++;
	}
	putchar('"');
}

static void print_diagnostics(const anixops_engine_t *engine)
{
	size_t count = anixops_engine_rule_diagnostic_count(engine);
	size_t i;
	printf("\"diagnostics\":[");
	for (i = 0; i < count; i++) {
		anixops_rule_diagnostic_t diagnostic;
		if (anixops_engine_copy_rule_diagnostic(engine, i, &diagnostic) != ANIXOPS_OK) {
			continue;
		}
		if (i != 0) {
			putchar(',');
		}
		printf("{\"status\":%d,\"profile\":%d,\"line\":%zu,\"section\":", diagnostic.status, diagnostic.profile, diagnostic.line);
		json_string(diagnostic.section);
		printf(",\"action\":");
		json_string(diagnostic.action);
		printf(",\"message\":");
		json_string(diagnostic.message);
		putchar('}');
	}
	putchar(']');
}

static int load_engine(const char *path, anixops_engine_t **out_engine, int *out_status)
{
	char *config = read_file(path);
	anixops_engine_t *engine;
	int status;
	if (config == NULL) {
		return 0;
	}
	engine = anixops_engine_new();
	if (engine == NULL) {
		free(config);
		return 0;
	}
	status = anixops_engine_load_config(engine, config);
	free(config);
	*out_engine = engine;
	*out_status = status;
	return 1;
}

static int cmd_scan(const char *path)
{
	anixops_engine_t *engine = NULL;
	int status = ANIXOPS_OK;
	int last_status = ANIXOPS_OK;
	size_t last_line = 0;
	char message[ANIXOPS_MESSAGE_CAP];

	if (!load_engine(path, &engine, &status)) {
		fprintf(stderr, "failed to load plugin: %s\n", path);
		return 1;
	}
	message[0] = '\0';
	(void)anixops_engine_copy_last_error(engine, &last_status, &last_line, message, sizeof(message));
	printf("{\"version\":");
	json_string(anixops_version());
	printf(",\"status\":%d,\"lastError\":{\"status\":%d,\"line\":%zu,\"message\":", status, last_status, last_line);
	json_string(message);
	printf("},\"counts\":{\"mitm\":%zu,\"rewrite\":%zu,\"script\":%zu,\"argument\":%zu},",
		anixops_engine_mitm_pattern_count(engine),
		anixops_engine_rewrite_rule_count(engine),
		anixops_engine_script_rule_count(engine),
		anixops_engine_argument_count(engine));
	print_diagnostics(engine);
	printf("}\n");
	anixops_engine_free(engine);
	return status == ANIXOPS_OK ? 0 : 2;
}

static int cmd_scan_corpus(const char *manifest_path)
{
	char *manifest = read_file(manifest_path);
	corpus_fixture_t fixtures[CORPUS_FIXTURE_CAP];
	size_t fixture_count = 0;
	size_t i;
	int passed = 1;

	if (manifest == NULL) {
		fprintf(stderr, "failed to load corpus manifest: %s\n", manifest_path);
		return 1;
	}
	if (!parse_corpus_manifest(manifest, fixtures, &fixture_count)) {
		fprintf(stderr, "failed to parse corpus manifest: %s\n", manifest_path);
		free(manifest);
		return 1;
	}
	free(manifest);

	printf("{\"version\":");
	json_string(anixops_version());
	printf(",\"manifest\":");
	json_string(manifest_path);
	printf(",\"fixtureCount\":%zu,\"fixtures\":[", fixture_count);
	for (i = 0; i < fixture_count; i++) {
		char resolved_path[2048];
		anixops_engine_t *engine = NULL;
		int status = ANIXOPS_ERR_INVALID_ARGUMENT;
		size_t mitm_count = 0;
		size_t rewrite_count = 0;
		size_t script_count = 0;
		size_t argument_count = 0;
		size_t diagnostic_count = 0;
		int matched = 0;
		if (i != 0) {
			putchar(',');
		}
		if (!resolve_manifest_path(manifest_path, fixtures[i].path, resolved_path, sizeof(resolved_path)) ||
			!load_engine(resolved_path, &engine, &status)) {
			passed = 0;
			printf("{\"name\":");
			json_string(fixtures[i].name);
			printf(",\"platform\":");
			json_string(fixtures[i].platform);
			printf(",\"source\":");
			json_string(fixtures[i].source);
			printf(",\"sha256\":");
			json_string(fixtures[i].sha256);
			printf(",\"path\":");
			json_string(fixtures[i].path);
			printf(",\"status\":%d,\"expectedStatus\":%d,\"matched\":false,\"message\":",
				status,
				fixtures[i].expected_status);
			json_string("fixture load failed");
			putchar('}');
			continue;
		}
		mitm_count = anixops_engine_mitm_pattern_count(engine);
		rewrite_count = anixops_engine_rewrite_rule_count(engine);
		script_count = anixops_engine_script_rule_count(engine);
		argument_count = anixops_engine_argument_count(engine);
		diagnostic_count = anixops_engine_rule_diagnostic_count(engine);
		matched =
			status == fixtures[i].expected_status &&
			mitm_count == fixtures[i].expected_mitm &&
			rewrite_count == fixtures[i].expected_rewrite &&
			script_count == fixtures[i].expected_script &&
			argument_count == fixtures[i].expected_argument;
		if (!matched) {
			passed = 0;
		}
		printf("{\"name\":");
		json_string(fixtures[i].name);
		printf(",\"platform\":");
		json_string(fixtures[i].platform);
		printf(",\"source\":");
		json_string(fixtures[i].source);
		printf(",\"sha256\":");
		json_string(fixtures[i].sha256);
		printf(",\"path\":");
		json_string(fixtures[i].path);
		printf(",\"status\":%d,\"expectedStatus\":%d,\"counts\":{\"mitm\":%zu,\"rewrite\":%zu,\"script\":%zu,\"argument\":%zu},",
			status,
			fixtures[i].expected_status,
			mitm_count,
			rewrite_count,
			script_count,
			argument_count);
		printf("\"expectedCounts\":{\"mitm\":%zu,\"rewrite\":%zu,\"script\":%zu,\"argument\":%zu},\"diagnosticCount\":%zu,\"matched\":%s}",
			fixtures[i].expected_mitm,
			fixtures[i].expected_rewrite,
			fixtures[i].expected_script,
			fixtures[i].expected_argument,
			diagnostic_count,
			matched ? "true" : "false");
		anixops_engine_free(engine);
	}
	printf("],\"passed\":%s}\n", passed ? "true" : "false");
	return passed ? 0 : 2;
}

static anixops_phase_t parse_phase(const char *phase)
{
	return phase != NULL && strcmp(phase, "response") == 0 ? ANIXOPS_PHASE_RESPONSE : ANIXOPS_PHASE_REQUEST;
}

static void print_rewrite_result(const anixops_rewrite_result_t *rewrite)
{
	printf("{\"action\":%d,\"statusCode\":%d,\"ruleIndex\":%d,\"value\":",
		rewrite->action,
		rewrite->status_code,
		rewrite->rule_index);
	json_string(rewrite->value);
	printf(",\"message\":");
	json_string(rewrite->message);
	putchar('}');
}

static void print_header_result(const anixops_header_rewrite_result_t *header)
{
	printf("{\"action\":%d,\"phase\":%d,\"ruleIndex\":%d,\"headerName\":",
		header->action,
		header->phase,
		header->rule_index);
	json_string(header->header_name);
	printf(",\"value\":");
	json_string(header->value);
	printf(",\"message\":");
	json_string(header->message);
	putchar('}');
}

static void print_script_result(const anixops_script_result_t *script)
{
	printf("{\"kind\":%d,\"requiresBody\":%d,\"ruleIndex\":%d,\"scriptPath\":",
		script->kind,
		script->requires_body,
		script->rule_index);
	json_string(script->script_path);
	printf(",\"tag\":");
	json_string(script->tag);
	printf(",\"argument\":");
	json_string(script->argument);
	printf(",\"message\":");
	json_string(script->message);
	putchar('}');
}

static int cmd_trace(int argc, char **argv)
{
	const char *plugin = NULL;
	const char *url = NULL;
	const char *phase_text = "request";
	anixops_phase_t phase;
	anixops_engine_t *engine = NULL;
	anixops_rewrite_plan_t plan;
	int status;
	int plan_status;
	int i;

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--plugin") == 0 && i + 1 < argc) {
			plugin = argv[++i];
		}
		else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
			url = argv[++i];
		}
		else if (strcmp(argv[i], "--phase") == 0 && i + 1 < argc) {
			phase_text = argv[++i];
		}
		else {
			fprintf(stderr, "unknown trace argument: %s\n", argv[i]);
			return 1;
		}
	}
	if (plugin == NULL || url == NULL) {
		fprintf(stderr, "usage: anixops-mitm-runner trace --plugin <file> --url <url> [--phase request|response]\n");
		return 1;
	}
	if (!load_engine(plugin, &engine, &status)) {
		fprintf(stderr, "failed to load plugin: %s\n", plugin);
		return 1;
	}
	phase = parse_phase(phase_text);
	plan_status = anixops_rewrite_build_plan(engine, url, phase, NULL, NULL, 0, NULL, &plan);
	printf("{\"version\":");
	json_string(anixops_version());
	printf(",\"loadStatus\":%d,\"url\":", status);
	json_string(url);
	printf(",\"phase\":");
	json_string(phase == ANIXOPS_PHASE_RESPONSE ? "response" : "request");
	printf(",\"planStatus\":%d,\"requiresBody\":%d,\"rewrite\":", plan_status, plan.requires_body);
	print_rewrite_result(&plan.rewrite);
	printf(",\"headers\":[");
	for (i = 0; i < (int)plan.header_rewrite_count; i++) {
		if (i != 0) {
			putchar(',');
		}
		print_header_result(&plan.header_rewrites[i]);
	}
	printf("],\"headersTruncated\":%d", plan.header_rewrite_truncated);
	printf(",\"script\":");
	print_script_result(&plan.script);
	printf("}\n");
	anixops_engine_free(engine);
	return status == ANIXOPS_OK ? 0 : 2;
}

static int split_tabs(char *line, char **fields, size_t fields_cap)
{
	size_t count = 0;
	char *cursor = line;
	while (count < fields_cap) {
		fields[count++] = cursor;
		cursor = strchr(cursor, '\t');
		if (cursor == NULL) {
			break;
		}
		*cursor = '\0';
		cursor++;
	}
	return (int)count;
}

static int append_raw(char *out, size_t out_cap, size_t *pos, const char *value)
{
	size_t len = strlen(value == NULL ? "" : value);
	if (*pos + len >= out_cap) {
		return 0;
	}
	memcpy(out + *pos, value == NULL ? "" : value, len);
	*pos += len;
	out[*pos] = '\0';
	return 1;
}

static int append_shell_quoted(char *out, size_t out_cap, size_t *pos, const char *value)
{
	const char *p = value == NULL ? "" : value;
	if (!append_raw(out, out_cap, pos, "'")) {
		return 0;
	}
	while (*p != '\0') {
		if (*p == '\'') {
			if (!append_raw(out, out_cap, pos, "'\\''")) {
				return 0;
			}
		}
		else {
			char ch[2];
			ch[0] = *p;
			ch[1] = '\0';
			if (!append_raw(out, out_cap, pos, ch)) {
				return 0;
			}
		}
		p++;
	}
	return append_raw(out, out_cap, pos, "'");
}

static int write_temp_body_file(const char *body, char *path, size_t path_cap)
{
	FILE *fp;
	int fd;
	char tmpl[256];
	const char *tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL || tmpdir[0] == '\0') {
		tmpdir = "/tmp";
	}
	if (snprintf(tmpl, sizeof(tmpl), "%s/anixops-replay-body-XXXXXX", tmpdir) >= (int)sizeof(tmpl)) {
		return 0;
	}
	fd = mkstemp(tmpl);
	if (fd < 0) {
		return 0;
	}
	fp = fdopen(fd, "wb");
	if (fp == NULL) {
		close(fd);
		remove(tmpl);
		return 0;
	}
	if (fputs(body == NULL ? "" : body, fp) < 0) {
		fclose(fp);
		remove(tmpl);
		return 0;
	}
	if (fclose(fp) != 0) {
		remove(tmpl);
		return 0;
	}
	if (strlen(tmpl) >= path_cap) {
		remove(tmpl);
		return 0;
	}
	strcpy(path, tmpl);
	return 1;
}

static int run_script_runner(
	const char *runner,
	const char *script_file,
	const char *phase,
	const char *url,
	const char *argument,
	const char *body,
	const char *timeout_ms,
	const char *store_path,
	char *out,
	size_t out_cap)
{
	char body_path[256];
	char command[32768];
	size_t pos = 0;
	FILE *pipe;
	size_t out_pos = 0;
	int rc;

	if (runner == NULL || script_file == NULL || phase == NULL || url == NULL || out == NULL || out_cap == 0) {
		return -1;
	}
	out[0] = '\0';
	if (!write_temp_body_file(body, body_path, sizeof(body_path))) {
		return -1;
	}
	command[0] = '\0';
	if (!append_raw(command, sizeof(command), &pos, "node ") ||
		!append_shell_quoted(command, sizeof(command), &pos, runner) ||
		!append_raw(command, sizeof(command), &pos, " --script ") ||
		!append_shell_quoted(command, sizeof(command), &pos, script_file) ||
		!append_raw(command, sizeof(command), &pos, " --phase ") ||
		!append_shell_quoted(command, sizeof(command), &pos, phase) ||
		!append_raw(command, sizeof(command), &pos, " --url ") ||
		!append_shell_quoted(command, sizeof(command), &pos, url) ||
		!append_raw(command, sizeof(command), &pos, " --argument ") ||
		!append_shell_quoted(command, sizeof(command), &pos, argument == NULL ? "" : argument) ||
		!append_raw(command, sizeof(command), &pos, " --body ") ||
		!append_shell_quoted(command, sizeof(command), &pos, body_path) ||
		!append_raw(command, sizeof(command), &pos, " --timeoutMs ") ||
		!append_shell_quoted(command, sizeof(command), &pos, timeout_ms == NULL ? "5000" : timeout_ms)) {
		remove(body_path);
		return -1;
	}
	if (store_path != NULL && store_path[0] != '\0') {
		if (!append_raw(command, sizeof(command), &pos, " --store ") ||
			!append_shell_quoted(command, sizeof(command), &pos, store_path)) {
			remove(body_path);
			return -1;
		}
	}
	if (strcmp(phase, "response") == 0) {
		if (!append_raw(command, sizeof(command), &pos, " --status 200")) {
			remove(body_path);
			return -1;
		}
	}
	pipe = popen(command, "r");
	if (pipe == NULL) {
		remove(body_path);
		return -1;
	}
	while (out_pos + 1 < out_cap) {
		size_t read_len = fread(out + out_pos, 1, out_cap - out_pos - 1, pipe);
		out_pos += read_len;
		if (read_len == 0) {
			break;
		}
	}
	out[out_pos] = '\0';
	while (out_pos > 0 && (out[out_pos - 1] == '\n' || out[out_pos - 1] == '\r')) {
		out[--out_pos] = '\0';
	}
	rc = pclose(pipe);
	remove(body_path);
	return rc == 0 ? 0 : rc;
}

static int json_extract_string_field(const char *json, const char *field, char *out, size_t out_cap)
{
	char needle[64];
	const char *p;
	size_t pos = 0;
	if (json == NULL || field == NULL || out == NULL || out_cap == 0) {
		return 0;
	}
	if (snprintf(needle, sizeof(needle), "\"%s\"", field) >= (int)sizeof(needle)) {
		return 0;
	}
	p = strstr(json, needle);
	if (p == NULL) {
		return 0;
	}
	p += strlen(needle);
	p = strchr(p, ':');
	if (p == NULL) {
		return 0;
	}
	p++;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
		p++;
	}
	if (*p != '"') {
		return 0;
	}
	p++;
	while (*p != '\0' && *p != '"') {
		char ch = *p++;
		if (ch == '\\') {
			ch = *p++;
			switch (ch) {
			case '"':
			case '\\':
			case '/':
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			default:
				if (ch == '\0') {
					return 0;
				}
				break;
			}
		}
		if (pos + 1 >= out_cap) {
			return 0;
		}
		out[pos++] = ch;
	}
	if (*p != '"') {
		return 0;
	}
	out[pos] = '\0';
	return 1;
}

static const char *find_script_map(const char *script_path, const char **maps, size_t map_count)
{
	size_t i;
	size_t path_len;
	if (script_path == NULL) {
		return NULL;
	}
	path_len = strlen(script_path);
	for (i = 0; i < map_count; i++) {
		if (strncmp(maps[i], script_path, path_len) == 0 && maps[i][path_len] == '=') {
			return maps[i] + path_len + 1;
		}
	}
	return NULL;
}

static int cmd_replay(int argc, char **argv)
{
	const char *plugin = NULL;
	const char *fixture = NULL;
	const char *script_runner = NULL;
	const char *timeout_ms = "5000";
	const char *script_store = NULL;
	const char *script_maps[SCRIPT_MAP_CAP];
	size_t script_map_count = 0;
	anixops_engine_t *engine = NULL;
	FILE *fp;
	char line[8192];
	size_t line_no = 0;
	int status;
	int i;
	int case_count = 0;

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--plugin") == 0 && i + 1 < argc) {
			plugin = argv[++i];
		}
		else if (strcmp(argv[i], "--fixture") == 0 && i + 1 < argc) {
			fixture = argv[++i];
		}
		else if (strcmp(argv[i], "--script-runner") == 0 && i + 1 < argc) {
			script_runner = argv[++i];
		}
		else if (strcmp(argv[i], "--script-map") == 0 && i + 1 < argc) {
			if (script_map_count >= SCRIPT_MAP_CAP) {
				fprintf(stderr, "too many --script-map entries\n");
				return 1;
			}
			script_maps[script_map_count++] = argv[++i];
		}
		else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
			timeout_ms = argv[++i];
		}
		else if (strcmp(argv[i], "--script-store") == 0 && i + 1 < argc) {
			script_store = argv[++i];
		}
		else {
			fprintf(stderr, "unknown replay argument: %s\n", argv[i]);
			return 1;
		}
	}
	if (plugin == NULL || fixture == NULL) {
		fprintf(stderr, "usage: anixops-mitm-runner replay --plugin <file> --fixture <cases.tsv>\n");
		return 1;
	}
	if (!load_engine(plugin, &engine, &status)) {
		fprintf(stderr, "failed to load plugin: %s\n", plugin);
		return 1;
	}
	fp = fopen(fixture, "rb");
	if (fp == NULL) {
		fprintf(stderr, "failed to load fixture: %s\n", fixture);
		anixops_engine_free(engine);
		return 1;
	}

	printf("{\"version\":");
	json_string(anixops_version());
	printf(",\"loadStatus\":%d,\"fixture\":", status);
	json_string(fixture);
	printf(",\"cases\":[");
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *fields[5];
		char *body = NULL;
		char out_body[8192];
		char final_body[8192];
		char runtime_done[8192];
		char runtime_message[ANIXOPS_MESSAGE_CAP];
		anixops_phase_t phase;
		anixops_rewrite_result_t rewrite;
		anixops_script_result_t script;
		int has_body;
		int runtime_status = -1;
		int runtime_present = 0;
		int field_count;
		size_t len;

		line_no++;
		len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}
		if (line[0] == '\0' || line[0] == '#') {
			continue;
		}
		field_count = split_tabs(line, fields, 5);
		if (field_count != 5 || strcmp(fields[0], "case") != 0) {
			fprintf(stderr, "invalid replay fixture line %zu\n", line_no);
			fclose(fp);
			anixops_engine_free(engine);
			return 2;
		}
		phase = parse_phase(fields[2]);
		body = fields[4];
		has_body = strcmp(body, "-") != 0;
		if (has_body) {
			(void)anixops_rewrite_apply_body(engine, fields[3], phase, body, out_body, sizeof(out_body), &rewrite);
		}
		else {
			out_body[0] = '\0';
			(void)anixops_rewrite_evaluate_url(engine, fields[3], phase, &rewrite);
		}
		(void)anixops_script_evaluate_url(engine, fields[3], phase, &script);
		if (has_body) {
			snprintf(final_body, sizeof(final_body), "%s", out_body);
		}
		else {
			final_body[0] = '\0';
		}
		runtime_done[0] = '\0';
		runtime_message[0] = '\0';
		if (script.kind != ANIXOPS_SCRIPT_NONE) {
			const char *script_file = find_script_map(script.script_path, script_maps, script_map_count);
			runtime_present = 1;
			if (script_runner == NULL) {
				snprintf(runtime_message, sizeof(runtime_message), "%s", "script runner not configured");
			}
			else if (script_file == NULL || script_file[0] == '\0') {
				snprintf(runtime_message, sizeof(runtime_message), "%s", "script map not found");
			}
			else if (!has_body) {
				snprintf(runtime_message, sizeof(runtime_message), "%s", "body unavailable");
			}
			else {
				runtime_status = run_script_runner(
					script_runner,
					script_file,
					phase == ANIXOPS_PHASE_RESPONSE ? "response" : "request",
					fields[3],
						script.argument,
						out_body,
						timeout_ms,
						script_store,
						runtime_done,
						sizeof(runtime_done));
				if (runtime_status == 0) {
					char script_body[8192];
					snprintf(runtime_message, sizeof(runtime_message), "%s", "script executed");
					if (json_extract_string_field(runtime_done, "body", script_body, sizeof(script_body))) {
						snprintf(final_body, sizeof(final_body), "%s", script_body);
					}
				}
				else {
					snprintf(runtime_message, sizeof(runtime_message), "%s", "script execution failed");
				}
			}
		}
		if (case_count != 0) {
			putchar(',');
		}
		printf("{\"name\":");
		json_string(fields[1]);
		printf(",\"phase\":");
		json_string(phase == ANIXOPS_PHASE_RESPONSE ? "response" : "request");
		printf(",\"url\":");
		json_string(fields[3]);
		printf(",\"rewrite\":");
		print_rewrite_result(&rewrite);
		printf(",\"script\":");
		print_script_result(&script);
		printf(",\"scriptRuntime\":");
		if (runtime_present) {
			printf("{\"status\":%d,\"message\":", runtime_status);
			json_string(runtime_message);
			printf(",\"doneText\":");
			json_string(runtime_done);
			putchar('}');
		}
		else {
			fputs("null", stdout);
		}
		printf(",\"body\":");
		if (has_body) {
			json_string(final_body);
		}
		else {
			fputs("null", stdout);
		}
		putchar('}');
		case_count++;
	}
	if (ferror(fp)) {
		fprintf(stderr, "failed reading fixture: %s\n", fixture);
		fclose(fp);
		anixops_engine_free(engine);
		return 1;
	}
	fclose(fp);
	printf("]}\n");
	anixops_engine_free(engine);
	return status == ANIXOPS_OK ? 0 : 2;
}

static void usage(void)
{
	fputs(
		"usage:\n"
		"  anixops-mitm-runner scan <plugin-file>\n"
		"  anixops-mitm-runner scan --corpus <manifest.json>\n"
		"  anixops-mitm-runner trace --plugin <file> --url <url> [--phase request|response]\n"
		"  anixops-mitm-runner replay --plugin <file> --fixture <cases.tsv> "
		"[--script-runner <node-runner.js> --script-map <url=file> --script-store <file>]\n",
		stderr);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}
	if (strcmp(argv[1], "scan") == 0) {
		if (argc == 3) {
			return cmd_scan(argv[2]);
		}
		if (argc == 4 && strcmp(argv[2], "--corpus") == 0) {
			return cmd_scan_corpus(argv[3]);
		}
		usage();
		return 1;
	}
	if (strcmp(argv[1], "trace") == 0) {
		return cmd_trace(argc, argv);
	}
	if (strcmp(argv[1], "replay") == 0) {
		return cmd_replay(argc, argv);
	}
	usage();
	return 1;
}
