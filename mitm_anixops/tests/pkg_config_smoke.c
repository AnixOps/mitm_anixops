#include "mitm_anixops.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	const char *version = anixops_version();
	anixops_engine_t *engine;
	if (version == NULL || strcmp(version, "0.44.0") != 0) {
		fprintf(stderr, "unexpected version: %s\n", version == NULL ? "(null)" : version);
		return 1;
	}
	engine = anixops_engine_new();
	if (engine == NULL) {
		fputs("engine allocation failed\n", stderr);
		return 1;
	}
	anixops_engine_free(engine);
	return 0;
}
