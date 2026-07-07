#include "mitm_anixops.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_all(const char *path)
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

int main(int argc, char **argv)
{
	const char *config_path;
	const char *url;
	char *config;
	anixops_engine_t *engine;
	anixops_script_result_t result;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <plugin-file> <url>\n", argv[0]);
		return 64;
	}
	config_path = argv[1];
	url = argv[2];

	config = read_all(config_path);
	if (config == NULL) {
		fprintf(stderr, "failed to read %s\n", config_path);
		return 66;
	}
	engine = anixops_engine_new();
	if (engine == NULL) {
		free(config);
		fprintf(stderr, "failed to allocate engine\n");
		return 70;
	}
	rc = anixops_engine_load_config(engine, config);
	free(config);
	if (rc != ANIXOPS_OK) {
		anixops_engine_free(engine);
		fprintf(stderr, "anixops_engine_load_config failed: %d\n", rc);
		return 65;
	}
	rc = anixops_script_evaluate_url(engine, url, ANIXOPS_PHASE_RESPONSE, &result);
	if (rc != ANIXOPS_OK) {
		anixops_engine_free(engine);
		fprintf(stderr, "anixops_script_evaluate_url failed: %d\n", rc);
		return 65;
	}
	if (result.kind == ANIXOPS_SCRIPT_NONE) {
		anixops_engine_free(engine);
		fprintf(stderr, "no script matched\n");
		return 1;
	}

	printf("kind=%d\n", (int)result.kind);
	printf("phase=%d\n", (int)result.phase);
	printf("requires_body=%d\n", result.requires_body);
	printf("rule_index=%d\n", result.rule_index);
	printf("pattern=%s\n", result.matched_pattern);
	printf("script_path=%s\n", result.script_path);
	printf("tag=%s\n", result.tag);
	printf("argument=%s\n", result.argument);

	anixops_engine_free(engine);
	return 0;
}
