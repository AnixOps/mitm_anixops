#ifndef ANIXOPS_MITM_LINK_H
#define ANIXOPS_MITM_LINK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(ANIXOPS_STATIC)
#    define ANIXOPS_API
#  elif defined(ANIXOPS_BUILDING_DLL)
#    define ANIXOPS_API __declspec(dllexport)
#  else
#    define ANIXOPS_API __declspec(dllimport)
#  endif
#else
#  define ANIXOPS_API __attribute__((visibility("default")))
#endif

#define ANIXOPS_VERSION_MAJOR 0
#define ANIXOPS_VERSION_MINOR 32
#define ANIXOPS_VERSION_PATCH 0

#define ANIXOPS_PATTERN_CAP 256
#define ANIXOPS_VALUE_CAP 2048
#define ANIXOPS_ARGUMENT_CAP 4096
#define ANIXOPS_MESSAGE_CAP 256

typedef struct anixops_engine anixops_engine_t;

typedef enum anixops_status {
	ANIXOPS_OK = 0,
	ANIXOPS_ERR_INVALID_ARGUMENT = -1,
	ANIXOPS_ERR_OUT_OF_MEMORY = -2,
	ANIXOPS_ERR_REGEX = -3,
	ANIXOPS_ERR_CAPACITY = -4,
	ANIXOPS_ERR_PARSE = -5
} anixops_status_t;

typedef enum anixops_cert_state {
	ANIXOPS_CERT_UNKNOWN = 0,
	ANIXOPS_CERT_NOT_INSTALLED = 1,
	ANIXOPS_CERT_INSTALLED_UNTRUSTED = 2,
	ANIXOPS_CERT_TRUSTED = 3,
	ANIXOPS_CERT_REVOKED = 4
} anixops_cert_state_t;

typedef enum anixops_mitm_decision_type {
	ANIXOPS_MITM_BYPASS = 0,
	ANIXOPS_MITM_INTERCEPT = 1,
	ANIXOPS_MITM_REJECT_QUIC = 2
} anixops_mitm_decision_type_t;

typedef enum anixops_mitm_reason {
	ANIXOPS_MITM_REASON_NONE = 0,
	ANIXOPS_MITM_REASON_DISABLED = 1,
	ANIXOPS_MITM_REASON_EMPTY_HOST = 2,
	ANIXOPS_MITM_REASON_CERT_NOT_TRUSTED = 3,
	ANIXOPS_MITM_REASON_DENY_HOST = 4,
	ANIXOPS_MITM_REASON_NO_HOST_MATCH = 5,
	ANIXOPS_MITM_REASON_QUIC_DISABLED_FOR_MITM = 6,
	ANIXOPS_MITM_REASON_ALLOWED = 7
} anixops_mitm_reason_t;

typedef enum anixops_phase {
	ANIXOPS_PHASE_REQUEST = 0,
	ANIXOPS_PHASE_RESPONSE = 1
} anixops_phase_t;

typedef enum anixops_rewrite_action {
	ANIXOPS_REWRITE_NONE = 0,
	ANIXOPS_REWRITE_REDIRECT_302 = 1,
	ANIXOPS_REWRITE_REDIRECT_307 = 2,
	ANIXOPS_REWRITE_REJECT = 3,
	ANIXOPS_REWRITE_REJECT_200 = 4,
	ANIXOPS_REWRITE_REJECT_IMG = 5,
	ANIXOPS_REWRITE_REJECT_VIDEO = 6,
	ANIXOPS_REWRITE_REJECT_DICT = 7,
	ANIXOPS_REWRITE_REJECT_ARRAY = 8,
	ANIXOPS_REWRITE_MOCK_REQUEST_BODY = 9,
	ANIXOPS_REWRITE_MOCK_RESPONSE_BODY = 10,
	ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX = 11,
	ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX = 12,
	ANIXOPS_REWRITE_HEADER_REPLACE = 13,
	ANIXOPS_REWRITE_HEADER_ADD = 14,
	ANIXOPS_REWRITE_HEADER_REPLACE_REGEX = 15,
	ANIXOPS_REWRITE_RESPONSE_HEADER_DEL = 16,
	ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE = 17,
	ANIXOPS_REWRITE_RESPONSE_HEADER_ADD = 18,
	ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX = 19,
	ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE = 20,
	ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE = 21,
	ANIXOPS_REWRITE_REDIRECT_301 = 22,
	ANIXOPS_REWRITE_REDIRECT_303 = 23,
	ANIXOPS_REWRITE_REDIRECT_308 = 24,
	ANIXOPS_REWRITE_HEADER_DEL = 25
} anixops_rewrite_action_t;

typedef enum anixops_script_kind {
	ANIXOPS_SCRIPT_NONE = 0,
	ANIXOPS_SCRIPT_HTTP_REQUEST = 1,
	ANIXOPS_SCRIPT_HTTP_RESPONSE = 2
} anixops_script_kind_t;

typedef struct anixops_mitm_decision {
	anixops_mitm_decision_type_t decision;
	anixops_mitm_reason_t reason;
	char matched_pattern[ANIXOPS_PATTERN_CAP];
	char message[ANIXOPS_MESSAGE_CAP];
} anixops_mitm_decision_t;

typedef struct anixops_rewrite_result {
	anixops_rewrite_action_t action;
	int status_code;
	int rule_index;
	char matched_pattern[ANIXOPS_PATTERN_CAP];
	char value[ANIXOPS_VALUE_CAP];
	char message[ANIXOPS_MESSAGE_CAP];
} anixops_rewrite_result_t;

typedef struct anixops_header_rewrite_result {
	anixops_rewrite_action_t action;
	anixops_phase_t phase;
	int rule_index;
	char matched_pattern[ANIXOPS_PATTERN_CAP];
	char header_name[ANIXOPS_PATTERN_CAP];
	char value[ANIXOPS_VALUE_CAP];
	char message[ANIXOPS_MESSAGE_CAP];
} anixops_header_rewrite_result_t;

typedef struct anixops_script_result {
	anixops_script_kind_t kind;
	anixops_phase_t phase;
	int requires_body;
	int rule_index;
	char matched_pattern[ANIXOPS_PATTERN_CAP];
	char script_path[ANIXOPS_VALUE_CAP];
	char tag[ANIXOPS_VALUE_CAP];
	char argument[ANIXOPS_ARGUMENT_CAP];
	char message[ANIXOPS_MESSAGE_CAP];
} anixops_script_result_t;

ANIXOPS_API const char *anixops_version(void);
ANIXOPS_API const char *anixops_status_message(int status);
ANIXOPS_API anixops_engine_t *anixops_engine_new(void);
ANIXOPS_API void anixops_engine_free(anixops_engine_t *engine);
ANIXOPS_API void anixops_engine_clear(anixops_engine_t *engine);

ANIXOPS_API int anixops_engine_copy_last_error(
	const anixops_engine_t *engine,
	int *out_status,
	size_t *out_line,
	char *out_message,
	size_t out_message_cap);

ANIXOPS_API int anixops_engine_load_config(anixops_engine_t *engine, const char *config_text);
ANIXOPS_API int anixops_engine_add_rewrite_rule(anixops_engine_t *engine, const char *line);
ANIXOPS_API int anixops_engine_add_script_rule(anixops_engine_t *engine, const char *line);
ANIXOPS_API int anixops_engine_add_argument(anixops_engine_t *engine, const char *line);
ANIXOPS_API int anixops_engine_set_argument_value(anixops_engine_t *engine, const char *name, const char *value);
ANIXOPS_API int anixops_engine_add_mitm_hostname(anixops_engine_t *engine, const char *pattern);

ANIXOPS_API void anixops_engine_set_mitm_enabled(anixops_engine_t *engine, int enabled);
ANIXOPS_API void anixops_engine_set_h2_mitm_enabled(anixops_engine_t *engine, int enabled);
ANIXOPS_API void anixops_engine_set_disable_quic_for_mitm(anixops_engine_t *engine, int enabled);
ANIXOPS_API void anixops_engine_set_cert_state(anixops_engine_t *engine, anixops_cert_state_t state);

ANIXOPS_API int anixops_mitm_evaluate(
	const anixops_engine_t *engine,
	const char *hostname,
	int is_quic,
	anixops_mitm_decision_t *out_decision);

ANIXOPS_API int anixops_rewrite_evaluate_url(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	anixops_rewrite_result_t *out_result);

ANIXOPS_API int anixops_rewrite_apply_body(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	const char *body,
	char *out_body,
	size_t out_body_cap,
	anixops_rewrite_result_t *out_result);

ANIXOPS_API int anixops_rewrite_evaluate_header(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	size_t start_index,
	const char *current_header_value,
	anixops_header_rewrite_result_t *out_result);

ANIXOPS_API int anixops_script_evaluate_url(
	const anixops_engine_t *engine,
	const char *url,
	anixops_phase_t phase,
	anixops_script_result_t *out_result);

ANIXOPS_API size_t anixops_engine_mitm_pattern_count(const anixops_engine_t *engine);
ANIXOPS_API size_t anixops_engine_rewrite_rule_count(const anixops_engine_t *engine);
ANIXOPS_API size_t anixops_engine_script_rule_count(const anixops_engine_t *engine);
ANIXOPS_API size_t anixops_engine_argument_count(const anixops_engine_t *engine);
ANIXOPS_API int anixops_engine_h2_mitm_enabled(const anixops_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
