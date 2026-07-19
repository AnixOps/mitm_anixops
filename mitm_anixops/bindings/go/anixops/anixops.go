package anixops

/*
#cgo pkg-config: mitm_anixops
#include "mitm_anixops.h"
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

type Status int

type Phase int

const (
	PhaseRequest  Phase = C.ANIXOPS_PHASE_REQUEST
	PhaseResponse Phase = C.ANIXOPS_PHASE_RESPONSE
)

const JQMaxInputBytesDefault uint = C.ANIXOPS_JQ_MAX_INPUT_BYTES_DEFAULT
const JQMaxOutputBytesDefault uint = C.ANIXOPS_JQ_MAX_OUTPUT_BYTES_DEFAULT
const JQMaxOutputValuesDefault uint = C.ANIXOPS_JQ_MAX_OUTPUT_VALUES_DEFAULT
const JQMaxExecutionTimeMsDefault uint = C.ANIXOPS_JQ_MAX_EXECUTION_TIME_MS_DEFAULT
const JQMaxMemoryBytesDefault uint = C.ANIXOPS_JQ_MAX_MEMORY_BYTES_DEFAULT
const MaxBodyBytesDefault uint = C.ANIXOPS_MAX_BODY_BYTES_DEFAULT
const JQFilterCacheCapacityDefault uint = C.ANIXOPS_JQ_FILTER_CACHE_CAPACITY_DEFAULT
const JQFilterCacheCapacityMax uint = C.ANIXOPS_JQ_FILTER_CACHE_CAPACITY_MAX

type RewriteAction int

const (
	RewriteNone                       RewriteAction = C.ANIXOPS_REWRITE_NONE
	RewriteURLRewrite200              RewriteAction = C.ANIXOPS_REWRITE_URL_REWRITE_200
	RewriteRedirect302                RewriteAction = C.ANIXOPS_REWRITE_REDIRECT_302
	RewriteRequestBodyReplaceRegex    RewriteAction = C.ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX
	RewriteResponseBodyReplaceRegex   RewriteAction = C.ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX
	RewriteHeaderDel                  RewriteAction = C.ANIXOPS_REWRITE_HEADER_DEL
	RewriteHeaderAdd                  RewriteAction = C.ANIXOPS_REWRITE_HEADER_ADD
	RewriteHeaderReplace              RewriteAction = C.ANIXOPS_REWRITE_HEADER_REPLACE
	RewriteHeaderReplaceRegex         RewriteAction = C.ANIXOPS_REWRITE_HEADER_REPLACE_REGEX
	RewriteResponseHeaderDel          RewriteAction = C.ANIXOPS_REWRITE_RESPONSE_HEADER_DEL
	RewriteResponseHeaderAdd          RewriteAction = C.ANIXOPS_REWRITE_RESPONSE_HEADER_ADD
	RewriteResponseHeaderReplace      RewriteAction = C.ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE
	RewriteResponseHeaderReplaceRegex RewriteAction = C.ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX
)

type ScriptKind int

const (
	ScriptNone         ScriptKind = C.ANIXOPS_SCRIPT_NONE
	ScriptHTTPRequest  ScriptKind = C.ANIXOPS_SCRIPT_HTTP_REQUEST
	ScriptHTTPResponse ScriptKind = C.ANIXOPS_SCRIPT_HTTP_RESPONSE
)

type Engine struct {
	ptr *C.anixops_engine_t
}

type RewriteResult struct {
	Action         RewriteAction
	StatusCode     int
	RuleIndex      int
	MatchedPattern string
	Value          string
	Message        string
}

type BodyRewriteChain struct {
	Rewrites  []RewriteResult
	Rewritten bool
	Truncated bool
}

type HeaderRewriteResult struct {
	Action         RewriteAction
	Phase          Phase
	RuleIndex      int
	MatchedPattern string
	HeaderName     string
	Value          string
	Message        string
}

type HeaderField struct {
	Name  string
	Value string
}

type HeaderList struct {
	Fields    []HeaderField
	Truncated bool
}

type ScriptResult struct {
	Kind           ScriptKind
	Phase          Phase
	RequiresBody   bool
	TimeoutMs      uint
	MaxSize        uint
	RuleIndex      int
	MatchedPattern string
	ScriptPath     string
	Tag            string
	Argument       string
	Message        string
}

type RewritePlan struct {
	Phase                  Phase
	BodyAvailable          bool
	RequiresBody           bool
	Rewrite                RewriteResult
	HeaderRewrites         []HeaderRewriteResult
	HeaderRewriteTruncated bool
	Script                 ScriptResult
}

func Version() string {
	return C.GoString(C.anixops_version())
}

func NewEngine() (*Engine, error) {
	ptr := C.anixops_engine_new()
	if ptr == nil {
		return nil, fmt.Errorf("anixops: engine allocation failed")
	}
	return &Engine{ptr: ptr}, nil
}

func (e *Engine) Close() {
	if e == nil || e.ptr == nil {
		return
	}
	C.anixops_engine_free(e.ptr)
	e.ptr = nil
}

func (e *Engine) SetJQMaxInputBytes(maxInputBytes uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq max input bytes", C.anixops_engine_set_jq_max_input_bytes(e.ptr, C.size_t(maxInputBytes)))
}

func (e *Engine) JQMaxInputBytes() uint {
	if e == nil || e.ptr == nil {
		return JQMaxInputBytesDefault
	}
	return uint(C.anixops_engine_jq_max_input_bytes(e.ptr))
}

func (e *Engine) SetJQMaxOutputBytes(maxOutputBytes uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq max output bytes", C.anixops_engine_set_jq_max_output_bytes(e.ptr, C.size_t(maxOutputBytes)))
}

func (e *Engine) JQMaxOutputBytes() uint {
	if e == nil || e.ptr == nil {
		return JQMaxOutputBytesDefault
	}
	return uint(C.anixops_engine_jq_max_output_bytes(e.ptr))
}

func (e *Engine) SetJQMaxOutputValues(maxOutputValues uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq max output values", C.anixops_engine_set_jq_max_output_values(e.ptr, C.size_t(maxOutputValues)))
}

func (e *Engine) JQMaxOutputValues() uint {
	if e == nil || e.ptr == nil {
		return JQMaxOutputValuesDefault
	}
	return uint(C.anixops_engine_jq_max_output_values(e.ptr))
}

func (e *Engine) SetJQMaxExecutionTimeMs(maxExecutionTimeMs uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq max execution time ms", C.anixops_engine_set_jq_max_execution_time_ms(e.ptr, C.size_t(maxExecutionTimeMs)))
}

func (e *Engine) JQMaxExecutionTimeMs() uint {
	if e == nil || e.ptr == nil {
		return JQMaxExecutionTimeMsDefault
	}
	return uint(C.anixops_engine_jq_max_execution_time_ms(e.ptr))
}

func (e *Engine) SetJQMaxMemoryBytes(maxMemoryBytes uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq max memory bytes", C.anixops_engine_set_jq_max_memory_bytes(e.ptr, C.size_t(maxMemoryBytes)))
}

func (e *Engine) JQMaxMemoryBytes() uint {
	if e == nil || e.ptr == nil {
		return JQMaxMemoryBytesDefault
	}
	return uint(C.anixops_engine_jq_max_memory_bytes(e.ptr))
}

func (e *Engine) SetMaxBodyBytes(maxBodyBytes uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set max body bytes", C.anixops_engine_set_max_body_bytes(e.ptr, C.size_t(maxBodyBytes)))
}

func (e *Engine) MaxBodyBytes() uint {
	if e == nil || e.ptr == nil {
		return MaxBodyBytesDefault
	}
	return uint(C.anixops_engine_max_body_bytes(e.ptr))
}

func (e *Engine) JQFilterCacheCount() uint {
	if e == nil || e.ptr == nil {
		return 0
	}
	return uint(C.anixops_engine_jq_filter_cache_count(e.ptr))
}

func (e *Engine) JQFilterCacheHitCount() uint {
	if e == nil || e.ptr == nil {
		return 0
	}
	return uint(C.anixops_engine_jq_filter_cache_hit_count(e.ptr))
}

func (e *Engine) SetJQFilterCacheCapacity(capacity uint) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("set jq filter cache capacity", C.anixops_engine_set_jq_filter_cache_capacity(e.ptr, C.size_t(capacity)))
}

func (e *Engine) JQFilterCacheCapacity() uint {
	if e == nil || e.ptr == nil {
		return JQFilterCacheCapacityDefault
	}
	return uint(C.anixops_engine_jq_filter_cache_capacity(e.ptr))
}

func (e *Engine) ClearJQFilterCache() error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	return statusError("clear jq filter cache", C.anixops_engine_clear_jq_filter_cache(e.ptr))
}

func (e *Engine) LoadConfig(config string) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("anixops: nil engine")
	}
	cconfig := C.CString(config)
	defer C.free(unsafe.Pointer(cconfig))
	return statusError("load config", C.anixops_engine_load_config(e.ptr, cconfig))
}

func (e *Engine) RewriteRuleCount() int {
	if e == nil || e.ptr == nil {
		return 0
	}
	return int(C.anixops_engine_rewrite_rule_count(e.ptr))
}

func (e *Engine) ScriptRuleCount() int {
	if e == nil || e.ptr == nil {
		return 0
	}
	return int(C.anixops_engine_script_rule_count(e.ptr))
}

func (e *Engine) EvaluateRewrite(url string, phase Phase) (RewriteResult, error) {
	var result C.anixops_rewrite_result_t
	if e == nil || e.ptr == nil {
		return RewriteResult{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))
	if err := statusError("evaluate rewrite", C.anixops_rewrite_evaluate_url(e.ptr, curl, C.anixops_phase_t(phase), &result)); err != nil {
		return RewriteResult{}, err
	}
	return rewriteResultFromC(&result), nil
}

func (e *Engine) ApplyBody(url string, phase Phase, body string) (string, RewriteResult, error) {
	var result C.anixops_rewrite_result_t
	outCap := len(body) + C.ANIXOPS_VALUE_CAP
	out := make([]byte, outCap)
	if e == nil || e.ptr == nil {
		return "", RewriteResult{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	cbody := C.CString(body)
	defer C.free(unsafe.Pointer(curl))
	defer C.free(unsafe.Pointer(cbody))
	if err := statusError(
		"apply body",
		C.anixops_rewrite_apply_body(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			cbody,
			(*C.char)(unsafe.Pointer(&out[0])),
			C.size_t(len(out)),
			&result)); err != nil {
		return "", RewriteResult{}, err
	}
	return C.GoString((*C.char)(unsafe.Pointer(&out[0]))), rewriteResultFromC(&result), nil
}

func (e *Engine) ApplyBodyBytes(url string, phase Phase, body []byte) ([]byte, RewriteResult, error) {
	var result C.anixops_rewrite_result_t
	var outLen C.size_t
	outCap := len(body) + C.ANIXOPS_VALUE_CAP
	out := make([]byte, outCap)
	if e == nil || e.ptr == nil {
		return nil, RewriteResult{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))
	var bodyPtr *C.uchar
	if len(body) != 0 {
		bodyPtr = (*C.uchar)(unsafe.Pointer(&body[0]))
	}
	if err := statusError(
		"apply body bytes",
		C.anixops_rewrite_apply_body_bytes(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			bodyPtr,
			C.size_t(len(body)),
			(*C.uchar)(unsafe.Pointer(&out[0])),
			C.size_t(len(out)),
			&outLen,
			&result)); err != nil {
		return nil, RewriteResult{}, err
	}
	if uint64(outLen) > uint64(len(out)) {
		return nil, RewriteResult{}, fmt.Errorf("anixops: body output length exceeds buffer")
	}
	return out[:int(outLen)], rewriteResultFromC(&result), nil
}

func (e *Engine) ApplyBodyChain(url string, phase Phase, body string) (string, BodyRewriteChain, error) {
	var chain C.anixops_body_rewrite_chain_t
	outCap := len(body) + C.ANIXOPS_VALUE_CAP
	out := make([]byte, outCap)
	if e == nil || e.ptr == nil {
		return "", BodyRewriteChain{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	cbody := C.CString(body)
	defer C.free(unsafe.Pointer(curl))
	defer C.free(unsafe.Pointer(cbody))
	if err := statusError(
		"apply body chain",
		C.anixops_rewrite_apply_body_chain(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			cbody,
			(*C.char)(unsafe.Pointer(&out[0])),
			C.size_t(len(out)),
			&chain)); err != nil {
		return "", BodyRewriteChain{}, err
	}
	return C.GoString((*C.char)(unsafe.Pointer(&out[0]))), bodyRewriteChainFromC(&chain), nil
}

func (e *Engine) ApplyBodyChainBytes(url string, phase Phase, body []byte) ([]byte, BodyRewriteChain, error) {
	var chain C.anixops_body_rewrite_chain_t
	var outLen C.size_t
	outCap := len(body) + C.ANIXOPS_VALUE_CAP
	out := make([]byte, outCap)
	if e == nil || e.ptr == nil {
		return nil, BodyRewriteChain{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))
	var bodyPtr *C.uchar
	if len(body) != 0 {
		bodyPtr = (*C.uchar)(unsafe.Pointer(&body[0]))
	}
	if err := statusError(
		"apply body chain bytes",
		C.anixops_rewrite_apply_body_chain_bytes(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			bodyPtr,
			C.size_t(len(body)),
			(*C.uchar)(unsafe.Pointer(&out[0])),
			C.size_t(len(out)),
			&outLen,
			&chain)); err != nil {
		return nil, BodyRewriteChain{}, err
	}
	if uint64(outLen) > uint64(len(out)) {
		return nil, BodyRewriteChain{}, fmt.Errorf("anixops: body output length exceeds buffer")
	}
	return out[:int(outLen)], bodyRewriteChainFromC(&chain), nil
}

func (e *Engine) EvaluateNamedHeader(
	url string,
	phase Phase,
	startIndex int,
	headerName string,
	currentHeaderValue string,
) (HeaderRewriteResult, error) {
	var result C.anixops_header_rewrite_result_t
	if e == nil || e.ptr == nil {
		return HeaderRewriteResult{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	cheaderName := C.CString(headerName)
	ccurrentHeaderValue := C.CString(currentHeaderValue)
	defer C.free(unsafe.Pointer(curl))
	defer C.free(unsafe.Pointer(cheaderName))
	defer C.free(unsafe.Pointer(ccurrentHeaderValue))
	if err := statusError(
		"evaluate named header",
		C.anixops_rewrite_evaluate_named_header(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			C.size_t(startIndex),
			cheaderName,
			ccurrentHeaderValue,
			&result)); err != nil {
		return HeaderRewriteResult{}, err
	}
	return headerRewriteResultFromC(&result), nil
}

func (e *Engine) ApplyHeaders(url string, phase Phase, headers HeaderList) (HeaderList, RewritePlan, error) {
	var in C.anixops_header_list_t
	var out C.anixops_header_list_t
	var plan C.anixops_rewrite_plan_t
	if e == nil || e.ptr == nil {
		return HeaderList{}, RewritePlan{}, fmt.Errorf("anixops: nil engine")
	}
	if len(headers.Fields) > int(C.ANIXOPS_HEADER_LIST_CAP) {
		return HeaderList{}, RewritePlan{}, fmt.Errorf("anixops: header list exceeds capacity")
	}
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))
	in.count = C.size_t(len(headers.Fields))
	if headers.Truncated {
		in.truncated = 1
	}
	for i, field := range headers.Fields {
		if writeCArrayString(unsafe.Pointer(&in.fields[i].name[0]), int(C.ANIXOPS_PATTERN_CAP), field.Name) {
			in.truncated = 1
		}
		if writeCArrayString(unsafe.Pointer(&in.fields[i].value[0]), int(C.ANIXOPS_VALUE_CAP), field.Value) {
			in.truncated = 1
		}
	}
	if err := statusError(
		"apply headers",
		C.anixops_rewrite_apply_headers(e.ptr, curl, C.anixops_phase_t(phase), &in, &out, &plan)); err != nil {
		return HeaderList{}, RewritePlan{}, err
	}
	return headerListFromC(&out), rewritePlanFromC(&plan), nil
}

func (e *Engine) BuildPlan(url string, phase Phase, body string) (RewritePlan, string, error) {
	return e.buildPlan(url, phase, body, nil)
}

func (e *Engine) BuildPlanWithCurrentHeader(
	url string,
	phase Phase,
	body string,
	currentHeaderValue string,
) (RewritePlan, string, error) {
	return e.buildPlan(url, phase, body, &currentHeaderValue)
}

func (e *Engine) buildPlan(
	url string,
	phase Phase,
	body string,
	currentHeaderValue *string,
) (RewritePlan, string, error) {
	var plan C.anixops_rewrite_plan_t
	outCap := len(body) + C.ANIXOPS_VALUE_CAP
	out := make([]byte, outCap)
	if e == nil || e.ptr == nil {
		return RewritePlan{}, "", fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	cbody := C.CString(body)
	var ccurrentHeaderValue *C.char
	defer C.free(unsafe.Pointer(curl))
	defer C.free(unsafe.Pointer(cbody))
	if currentHeaderValue != nil {
		ccurrentHeaderValue = C.CString(*currentHeaderValue)
		defer C.free(unsafe.Pointer(ccurrentHeaderValue))
	}
	if err := statusError(
		"build plan",
		C.anixops_rewrite_build_plan(
			e.ptr,
			curl,
			C.anixops_phase_t(phase),
			cbody,
			(*C.char)(unsafe.Pointer(&out[0])),
			C.size_t(len(out)),
			ccurrentHeaderValue,
			&plan)); err != nil {
		return RewritePlan{}, "", err
	}
	return rewritePlanFromC(&plan), C.GoString((*C.char)(unsafe.Pointer(&out[0]))), nil
}

func (e *Engine) EvaluateScript(url string, phase Phase) (ScriptResult, error) {
	var result C.anixops_script_result_t
	if e == nil || e.ptr == nil {
		return ScriptResult{}, fmt.Errorf("anixops: nil engine")
	}
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))
	if err := statusError("evaluate script", C.anixops_script_evaluate_url(e.ptr, curl, C.anixops_phase_t(phase), &result)); err != nil {
		return ScriptResult{}, err
	}
	return scriptResultFromC(&result), nil
}

func statusError(op string, status C.int) error {
	if status == C.ANIXOPS_OK {
		return nil
	}
	return fmt.Errorf("anixops: %s failed: %s (%d)", op, C.GoString(C.anixops_status_message(status)), int(status))
}

func rewriteResultFromC(result *C.anixops_rewrite_result_t) RewriteResult {
	return RewriteResult{
		Action:         RewriteAction(result.action),
		StatusCode:     int(result.status_code),
		RuleIndex:      int(result.rule_index),
		MatchedPattern: cArrayString(unsafe.Pointer(&result.matched_pattern[0])),
		Value:          cArrayString(unsafe.Pointer(&result.value[0])),
		Message:        cArrayString(unsafe.Pointer(&result.message[0])),
	}
}

func bodyRewriteChainFromC(chain *C.anixops_body_rewrite_chain_t) BodyRewriteChain {
	count := int(chain.rewrite_count)
	if count > int(C.ANIXOPS_BODY_CHAIN_CAP) {
		count = int(C.ANIXOPS_BODY_CHAIN_CAP)
	}
	rewrites := make([]RewriteResult, 0, count)
	for i := 0; i < count; i++ {
		rewrites = append(rewrites, rewriteResultFromC(&chain.rewrites[i]))
	}
	return BodyRewriteChain{
		Rewrites:  rewrites,
		Rewritten: chain.rewritten != 0,
		Truncated: chain.truncated != 0,
	}
}

func headerRewriteResultFromC(result *C.anixops_header_rewrite_result_t) HeaderRewriteResult {
	return HeaderRewriteResult{
		Action:         RewriteAction(result.action),
		Phase:          Phase(result.phase),
		RuleIndex:      int(result.rule_index),
		MatchedPattern: cArrayString(unsafe.Pointer(&result.matched_pattern[0])),
		HeaderName:     cArrayString(unsafe.Pointer(&result.header_name[0])),
		Value:          cArrayString(unsafe.Pointer(&result.value[0])),
		Message:        cArrayString(unsafe.Pointer(&result.message[0])),
	}
}

func headerListFromC(list *C.anixops_header_list_t) HeaderList {
	count := int(list.count)
	if count > int(C.ANIXOPS_HEADER_LIST_CAP) {
		count = int(C.ANIXOPS_HEADER_LIST_CAP)
	}
	fields := make([]HeaderField, 0, count)
	for i := 0; i < count; i++ {
		fields = append(fields, HeaderField{
			Name:  cArrayString(unsafe.Pointer(&list.fields[i].name[0])),
			Value: cArrayString(unsafe.Pointer(&list.fields[i].value[0])),
		})
	}
	return HeaderList{
		Fields:    fields,
		Truncated: list.truncated != 0,
	}
}

func scriptResultFromC(result *C.anixops_script_result_t) ScriptResult {
	return ScriptResult{
		Kind:           ScriptKind(result.kind),
		Phase:          Phase(result.phase),
		RequiresBody:   result.requires_body != 0,
		TimeoutMs:      uint(result.timeout_ms),
		MaxSize:        uint(result.max_size),
		RuleIndex:      int(result.rule_index),
		MatchedPattern: cArrayString(unsafe.Pointer(&result.matched_pattern[0])),
		ScriptPath:     cArrayString(unsafe.Pointer(&result.script_path[0])),
		Tag:            cArrayString(unsafe.Pointer(&result.tag[0])),
		Argument:       cArrayString(unsafe.Pointer(&result.argument[0])),
		Message:        cArrayString(unsafe.Pointer(&result.message[0])),
	}
}

func rewritePlanFromC(plan *C.anixops_rewrite_plan_t) RewritePlan {
	count := int(plan.header_rewrite_count)
	if count > int(C.ANIXOPS_PLAN_HEADER_CAP) {
		count = int(C.ANIXOPS_PLAN_HEADER_CAP)
	}
	headers := make([]HeaderRewriteResult, 0, count)
	for i := 0; i < count; i++ {
		headers = append(headers, headerRewriteResultFromC(&plan.header_rewrites[i]))
	}
	return RewritePlan{
		Phase:                  Phase(plan.phase),
		BodyAvailable:          plan.body_available != 0,
		RequiresBody:           plan.requires_body != 0,
		Rewrite:                rewriteResultFromC(&plan.rewrite),
		HeaderRewrites:         headers,
		HeaderRewriteTruncated: plan.header_rewrite_truncated != 0,
		Script:                 scriptResultFromC(&plan.script),
	}
}

func cArrayString(ptr unsafe.Pointer) string {
	return C.GoString((*C.char)(ptr))
}

func writeCArrayString(ptr unsafe.Pointer, cap int, value string) bool {
	bytes := []byte(value)
	truncated := false
	if len(bytes) >= cap {
		bytes = bytes[:cap-1]
		truncated = true
	}
	C.memset(ptr, 0, C.size_t(cap))
	copy(unsafe.Slice((*byte)(ptr), cap), bytes)
	return truncated
}
