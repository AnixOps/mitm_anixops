package anixops

import (
	"os"
	"testing"
)

const fixtureConfig = `
[Argument]
Mode = select,go

[Rewrite]
^http:\/\/old\.go\.example\/(.*) https://api.go.example/$1 302
^https:\/\/api\.go\.example\/v1 response-header-add X-Go go-plan
^https:\/\/api\.go\.example\/v1 response-body-replace-regex from to

[Script]
http-response ^https:\/\/api\.go\.example\/v1 requires-body=1, timeout=4, max-size=2048, script-path=https://scripts.example/go-response.js, tag=go.response, argument=[{Mode}]
`

func TestGoBindingEvaluatesPolicy(t *testing.T) {
	if Version() != "0.45.10" {
		t.Fatalf("Version() = %q", Version())
	}
	if PolicyCapabilityABIVersion() != PolicyCapabilityQueryABIVersion {
		t.Fatalf("PolicyCapabilityABIVersion() = %d", PolicyCapabilityABIVersion())
	}
	capabilities := PolicyCapabilities()
	for _, capability := range []PolicyCapability{
		PolicyCapabilityMITMDecision,
		PolicyCapabilityURLRewrite,
		PolicyCapabilityHeaderMutation,
		PolicyCapabilityBodyMutationBytes,
		PolicyCapabilityScriptDispatchMetadata,
		PolicyCapabilityRuleDiagnostics,
		PolicyCapabilityTaskDescriptorMetadata,
	} {
		if !capabilities.Supports(capability) {
			t.Fatalf("PolicyCapabilities() does not support %#x", capability)
		}
	}
	if !capabilities.Supports(PolicyCapabilityAllV1) {
		t.Fatal("PolicyCapabilities() does not support all V1 policy capabilities")
	}
	if !capabilities.Supports(0) {
		t.Fatal("PolicyCapabilities() does not support an empty requirement")
	}
	if capabilities.Supports(PolicyCapability(1) << 63) {
		t.Fatal("PolicyCapabilities() unexpectedly supports bit 63")
	}
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()
	if engine.JQMaxInputBytes() != JQMaxInputBytesDefault {
		t.Fatalf("JQMaxInputBytes() = %d", engine.JQMaxInputBytes())
	}
	if engine.JQMaxOutputBytes() != JQMaxOutputBytesDefault {
		t.Fatalf("JQMaxOutputBytes() = %d", engine.JQMaxOutputBytes())
	}
	if engine.JQMaxOutputValues() != JQMaxOutputValuesDefault {
		t.Fatalf("JQMaxOutputValues() = %d", engine.JQMaxOutputValues())
	}
	if engine.JQMaxExecutionTimeMs() != JQMaxExecutionTimeMsDefault {
		t.Fatalf("JQMaxExecutionTimeMs() = %d", engine.JQMaxExecutionTimeMs())
	}
	if engine.JQMaxMemoryBytes() != JQMaxMemoryBytesDefault {
		t.Fatalf("JQMaxMemoryBytes() = %d", engine.JQMaxMemoryBytes())
	}
	if MaxBodyBytesDefault != 0 {
		t.Fatalf("MaxBodyBytesDefault = %d", MaxBodyBytesDefault)
	}
	if engine.MaxBodyBytes() != MaxBodyBytesDefault {
		t.Fatalf("MaxBodyBytes() = %d", engine.MaxBodyBytes())
	}
	if err := engine.SetMaxBodyBytes(4096); err != nil {
		t.Fatal(err)
	}
	if engine.MaxBodyBytes() != 4096 {
		t.Fatalf("MaxBodyBytes() = %d", engine.MaxBodyBytes())
	}
	if err := engine.SetMaxBodyBytes(0); err != nil {
		t.Fatal(err)
	}
	if engine.JQFilterCacheCount() != 0 || engine.JQFilterCacheHitCount() != 0 {
		t.Fatalf("unexpected JQ filter cache state: count=%d hits=%d", engine.JQFilterCacheCount(), engine.JQFilterCacheHitCount())
	}
	if JQFilterCacheCapacityDefault != 4 {
		t.Fatalf("JQFilterCacheCapacityDefault = %d", JQFilterCacheCapacityDefault)
	}
	if JQFilterCacheCapacityMax != 16 {
		t.Fatalf("JQFilterCacheCapacityMax = %d", JQFilterCacheCapacityMax)
	}
	if engine.JQFilterCacheCapacity() != JQFilterCacheCapacityDefault {
		t.Fatalf("JQFilterCacheCapacity() = %d", engine.JQFilterCacheCapacity())
	}
	if err := engine.SetJQFilterCacheCapacity(2); err != nil {
		t.Fatal(err)
	}
	if engine.JQFilterCacheCapacity() != 2 {
		t.Fatalf("JQFilterCacheCapacity() = %d", engine.JQFilterCacheCapacity())
	}
	if err := engine.ClearJQFilterCache(); err != nil {
		t.Fatal(err)
	}
	if err := engine.SetJQMaxInputBytes(4096); err != nil {
		t.Fatal(err)
	}
	if engine.JQMaxInputBytes() != 4096 {
		t.Fatalf("JQMaxInputBytes() = %d", engine.JQMaxInputBytes())
	}
	if err := engine.SetJQMaxOutputBytes(8192); err != nil {
		t.Fatal(err)
	}
	if engine.JQMaxOutputBytes() != 8192 {
		t.Fatalf("JQMaxOutputBytes() = %d", engine.JQMaxOutputBytes())
	}
	if err := engine.SetJQMaxOutputValues(8); err != nil {
		t.Fatal(err)
	}
	if engine.JQMaxOutputValues() != 8 {
		t.Fatalf("JQMaxOutputValues() = %d", engine.JQMaxOutputValues())
	}
	if err := engine.SetJQMaxExecutionTimeMs(250); err != nil {
		t.Fatal(err)
	}
	if engine.JQMaxExecutionTimeMs() != 250 {
		t.Fatalf("JQMaxExecutionTimeMs() = %d", engine.JQMaxExecutionTimeMs())
	}
	if err := engine.SetJQMaxMemoryBytes(32 * 1024 * 1024); err != nil {
		t.Fatal(err)
	}
	if engine.JQMaxMemoryBytes() != 32*1024*1024 {
		t.Fatalf("JQMaxMemoryBytes() = %d", engine.JQMaxMemoryBytes())
	}

	if err := engine.LoadConfig(fixtureConfig); err != nil {
		t.Fatal(err)
	}
	if engine.RewriteRuleCount() != 3 {
		t.Fatalf("RewriteRuleCount() = %d", engine.RewriteRuleCount())
	}
	if engine.ScriptRuleCount() != 1 {
		t.Fatalf("ScriptRuleCount() = %d", engine.ScriptRuleCount())
	}

	rewrite, err := engine.EvaluateRewrite("http://old.go.example/v1", PhaseRequest)
	if err != nil {
		t.Fatal(err)
	}
	if rewrite.Action != RewriteRedirect302 || rewrite.StatusCode != 302 {
		t.Fatalf("unexpected rewrite: %+v", rewrite)
	}
	if rewrite.Value != "https://api.go.example/v1" {
		t.Fatalf("rewrite.Value = %q", rewrite.Value)
	}

	body, bodyRewrite, err := engine.ApplyBody("https://api.go.example/v1", PhaseResponse, "from=1")
	if err != nil {
		t.Fatal(err)
	}
	if body != "to=1" {
		t.Fatalf("body = %q", body)
	}
	if bodyRewrite.Message != "body rewritten" {
		t.Fatalf("body rewrite message = %q", bodyRewrite.Message)
	}

	namedHeader, err := engine.EvaluateNamedHeader("https://api.go.example/v1", PhaseResponse, 0, "x-go", "")
	if err != nil {
		t.Fatal(err)
	}
	if namedHeader.Action != RewriteResponseHeaderAdd || namedHeader.HeaderName != "X-Go" || namedHeader.Value != "go-plan" {
		t.Fatalf("unexpected named header: %+v", namedHeader)
	}

	missingHeader, err := engine.EvaluateNamedHeader("https://api.go.example/v1", PhaseResponse, 0, "x-missing", "")
	if err != nil {
		t.Fatal(err)
	}
	if missingHeader.Action != RewriteNone {
		t.Fatalf("unexpected missing named header: %+v", missingHeader)
	}

	plan, planBody, err := engine.BuildPlan("https://api.go.example/v1", PhaseResponse, "from=1")
	if err != nil {
		t.Fatal(err)
	}
	if !plan.BodyAvailable || !plan.RequiresBody {
		t.Fatalf("unexpected plan body flags: %+v", plan)
	}
	if planBody != "to=1" {
		t.Fatalf("plan body = %q", planBody)
	}
	if plan.Rewrite.Action != RewriteResponseBodyReplaceRegex || plan.Rewrite.RuleIndex != 2 {
		t.Fatalf("unexpected plan rewrite: %+v", plan.Rewrite)
	}
	if len(plan.HeaderRewrites) != 1 || plan.HeaderRewrites[0].Action != RewriteResponseHeaderAdd {
		t.Fatalf("unexpected plan headers: %+v", plan.HeaderRewrites)
	}
	if plan.HeaderRewrites[0].HeaderName != "X-Go" || plan.HeaderRewrites[0].Value != "go-plan" {
		t.Fatalf("unexpected plan header: %+v", plan.HeaderRewrites[0])
	}
	if plan.HeaderRewriteTruncated {
		t.Fatalf("plan headers truncated")
	}
	if plan.Script.Kind != ScriptHTTPResponse || plan.Script.ScriptPath != "https://scripts.example/go-response.js" {
		t.Fatalf("unexpected plan script: %+v", plan.Script)
	}
	if plan.Script.TimeoutMs != 4000 || plan.Script.MaxSize != 2048 {
		t.Fatalf("unexpected plan script scheduling attrs: %+v", plan.Script)
	}

	script, err := engine.EvaluateScript("https://api.go.example/v1", PhaseResponse)
	if err != nil {
		t.Fatal(err)
	}
	if script.Kind != ScriptHTTPResponse || !script.RequiresBody {
		t.Fatalf("unexpected script result: %+v", script)
	}
	if script.TimeoutMs != 4000 || script.MaxSize != 2048 {
		t.Fatalf("unexpected script scheduling attrs: %+v", script)
	}
	if script.ScriptPath != "https://scripts.example/go-response.js" {
		t.Fatalf("script.ScriptPath = %q", script.ScriptPath)
	}
	if script.Tag != "go.response" {
		t.Fatalf("script.Tag = %q", script.Tag)
	}
	if script.Argument != "Mode=go" {
		t.Fatalf("script.Argument = %q", script.Argument)
	}
}

func TestGoBindingLoadsSharedParityFixture(t *testing.T) {
	config, err := os.ReadFile("../../../tests/fixtures/BindingParity.Common.conf")
	if err != nil {
		t.Fatal(err)
	}
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()

	if err := engine.LoadConfig(string(config)); err != nil {
		t.Fatal(err)
	}
	if engine.RewriteRuleCount() != 7 {
		t.Fatalf("RewriteRuleCount() = %d", engine.RewriteRuleCount())
	}
	if engine.ScriptRuleCount() != 2 {
		t.Fatalf("ScriptRuleCount() = %d", engine.ScriptRuleCount())
	}

	redirect, err := engine.EvaluateRewrite("http://old.binding.test/item", PhaseRequest)
	if err != nil {
		t.Fatal(err)
	}
	if redirect.Action != RewriteRedirect302 || redirect.Value != "https://api.binding.test/item" {
		t.Fatalf("unexpected redirect: %+v", redirect)
	}

	requestPlan, requestBody, err := engine.BuildPlan("https://api.binding.test/request/item", PhaseRequest, "token=42")
	if err != nil {
		t.Fatal(err)
	}
	if requestBody != "token=42-bound" {
		t.Fatalf("request body = %q", requestBody)
	}
	if !requestPlan.BodyAvailable || !requestPlan.RequiresBody {
		t.Fatalf("unexpected request plan body flags: %+v", requestPlan)
	}
	if requestPlan.Rewrite.Action != RewriteRequestBodyReplaceRegex {
		t.Fatalf("unexpected request rewrite: %+v", requestPlan.Rewrite)
	}
	if len(requestPlan.HeaderRewrites) != 1 ||
		requestPlan.HeaderRewrites[0].Action != RewriteHeaderAdd ||
		requestPlan.HeaderRewrites[0].HeaderName != "X-Binding" ||
		requestPlan.HeaderRewrites[0].Value != "trace-item" {
		t.Fatalf("unexpected request headers: %+v", requestPlan.HeaderRewrites)
	}
	if requestPlan.Script.Kind != ScriptHTTPRequest ||
		requestPlan.Script.Tag != "binding.request" ||
		requestPlan.Script.ScriptPath != "https://scripts.binding.test/request.js" ||
		requestPlan.Script.Argument != "Mode=binding" ||
		requestPlan.Script.TimeoutMs != 333 ||
		requestPlan.Script.MaxSize != 777 {
		t.Fatalf("unexpected request script: %+v", requestPlan.Script)
	}
	requestHeader, err := engine.EvaluateNamedHeader(
		"https://api.binding.test/request-current/item",
		PhaseRequest,
		0,
		"x-binding-current",
		"old-item",
	)
	if err != nil {
		t.Fatal(err)
	}
	if requestHeader.Action != RewriteHeaderReplaceRegex ||
		requestHeader.HeaderName != "X-Binding-Current" ||
		requestHeader.Value != "req-item" {
		t.Fatalf("unexpected request named header: %+v", requestHeader)
	}
	requestHeaderPlan, _, err := engine.BuildPlanWithCurrentHeader(
		"https://api.binding.test/request-current/item",
		PhaseRequest,
		"",
		"old-item",
	)
	if err != nil {
		t.Fatal(err)
	}
	if len(requestHeaderPlan.HeaderRewrites) != 1 ||
		requestHeaderPlan.HeaderRewrites[0].Action != requestHeader.Action ||
		requestHeaderPlan.HeaderRewrites[0].HeaderName != requestHeader.HeaderName ||
		requestHeaderPlan.HeaderRewrites[0].Value != requestHeader.Value {
		t.Fatalf("unexpected request current-header plan: %+v", requestHeaderPlan.HeaderRewrites)
	}

	responsePlan, responseBody, err := engine.BuildPlan("https://api.binding.test/response/item", PhaseResponse, "ad=1&ok=1")
	if err != nil {
		t.Fatal(err)
	}
	if responseBody != "clean=1&ok=1" {
		t.Fatalf("response body = %q", responseBody)
	}
	if responsePlan.Rewrite.Action != RewriteResponseBodyReplaceRegex {
		t.Fatalf("unexpected response rewrite: %+v", responsePlan.Rewrite)
	}
	if len(responsePlan.HeaderRewrites) != 1 ||
		responsePlan.HeaderRewrites[0].Action != RewriteResponseHeaderAdd ||
		responsePlan.HeaderRewrites[0].HeaderName != "X-Binding" ||
		responsePlan.HeaderRewrites[0].Value != "resp-item" {
		t.Fatalf("unexpected response headers: %+v", responsePlan.HeaderRewrites)
	}
	if responsePlan.Script.Kind != ScriptHTTPResponse ||
		responsePlan.Script.Tag != "binding.response" ||
		responsePlan.Script.ScriptPath != "https://scripts.binding.test/response.js" ||
		responsePlan.Script.Argument != "Mode=binding" ||
		responsePlan.Script.TimeoutMs != 444 ||
		responsePlan.Script.MaxSize != 888 {
		t.Fatalf("unexpected response script: %+v", responsePlan.Script)
	}
	responseHeader, err := engine.EvaluateNamedHeader(
		"https://api.binding.test/response-current/item",
		PhaseResponse,
		0,
		"x-binding-current",
		"old-item",
	)
	if err != nil {
		t.Fatal(err)
	}
	if responseHeader.Action != RewriteResponseHeaderReplaceRegex ||
		responseHeader.HeaderName != "X-Binding-Current" ||
		responseHeader.Value != "resp-item" {
		t.Fatalf("unexpected response named header: %+v", responseHeader)
	}
	responseHeaderPlan, _, err := engine.BuildPlanWithCurrentHeader(
		"https://api.binding.test/response-current/item",
		PhaseResponse,
		"",
		"old-item",
	)
	if err != nil {
		t.Fatal(err)
	}
	if len(responseHeaderPlan.HeaderRewrites) != 1 ||
		responseHeaderPlan.HeaderRewrites[0].Action != responseHeader.Action ||
		responseHeaderPlan.HeaderRewrites[0].HeaderName != responseHeader.HeaderName ||
		responseHeaderPlan.HeaderRewrites[0].Value != responseHeader.Value {
		t.Fatalf("unexpected response current-header plan: %+v", responseHeaderPlan.HeaderRewrites)
	}
}

func TestGoBindingAppliesHeaderList(t *testing.T) {
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()

	const config = `
[Rewrite]
^https:\/\/api\.go\.example\/cookies response-header-add Set-Cookie "c=1; Path=/"
^https:\/\/api\.go\.example\/cookies response-header-replace-regex Set-Cookie "a=1" "a=2"
^https:\/\/api\.go\.example\/cookies response-header-del X-Remove
`
	if err := engine.LoadConfig(config); err != nil {
		t.Fatal(err)
	}
	headers, plan, err := engine.ApplyHeaders("https://api.go.example/cookies", PhaseResponse, HeaderList{
		Fields: []HeaderField{
			{Name: "Set-Cookie", Value: "a=1; Path=/"},
			{Name: "set-cookie", Value: "b=1; Path=/"},
			{Name: "X-Remove", Value: "yes"},
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	if headers.Truncated {
		t.Fatalf("headers truncated")
	}
	if len(headers.Fields) != 3 {
		t.Fatalf("len(headers.Fields) = %d: %+v", len(headers.Fields), headers.Fields)
	}
	want := []HeaderField{
		{Name: "Set-Cookie", Value: "a=2; Path=/"},
		{Name: "set-cookie", Value: "b=1; Path=/"},
		{Name: "Set-Cookie", Value: "c=1; Path=/"},
	}
	for i := range want {
		if headers.Fields[i] != want[i] {
			t.Fatalf("headers.Fields[%d] = %+v, want %+v", i, headers.Fields[i], want[i])
		}
	}
	if len(plan.HeaderRewrites) != 3 {
		t.Fatalf("len(plan.HeaderRewrites) = %d", len(plan.HeaderRewrites))
	}
	if plan.HeaderRewrites[0].Action != RewriteResponseHeaderAdd ||
		plan.HeaderRewrites[1].Action != RewriteResponseHeaderReplaceRegex ||
		plan.HeaderRewrites[2].Action != RewriteResponseHeaderDel {
		t.Fatalf("unexpected plan headers: %+v", plan.HeaderRewrites)
	}
}

func TestGoBindingAppliesBodyChain(t *testing.T) {
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()

	const config = `
[Rewrite]
^https:\/\/api\.go\.example\/chain request-body-replace-regex "from=([0-9]+)" "mid=$1"
^https:\/\/api\.go\.example\/chain request-body-replace-regex "mid=([0-9]+)" "to=$1"
`
	if err := engine.LoadConfig(config); err != nil {
		t.Fatal(err)
	}
	body, chain, err := engine.ApplyBodyChain("https://api.go.example/chain", PhaseRequest, "from=42")
	if err != nil {
		t.Fatal(err)
	}
	if body != "to=42" {
		t.Fatalf("body = %q", body)
	}
	if !chain.Rewritten || chain.Truncated {
		t.Fatalf("unexpected chain flags: %+v", chain)
	}
	if len(chain.Rewrites) != 2 {
		t.Fatalf("len(chain.Rewrites) = %d", len(chain.Rewrites))
	}
	if chain.Rewrites[0].Action != RewriteRequestBodyReplaceRegex ||
		chain.Rewrites[1].Action != RewriteRequestBodyReplaceRegex {
		t.Fatalf("unexpected chain rewrites: %+v", chain.Rewrites)
	}
}

func TestGoBindingAppliesBodyBytes(t *testing.T) {
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()

	if err := engine.LoadConfig(`
[Rewrite]
^https:\/\/api\.go\.example\/bytes request-body-replace-regex "from=([0-9]+)" "to=$1"
`); err != nil {
		t.Fatal(err)
	}
	body, rewrite, err := engine.ApplyBodyBytes(
		"https://api.go.example/bytes",
		PhaseRequest,
		[]byte{'f', 'r', 'o', 'm', '=', '1', 0, 'x'},
	)
	if err != nil {
		t.Fatal(err)
	}
	if string(body) != "from=1\x00x" {
		t.Fatalf("body = %q", body)
	}
	if rewrite.Message != "binary body passthrough" {
		t.Fatalf("rewrite message = %q", rewrite.Message)
	}
	chainBody, chain, err := engine.ApplyBodyChainBytes(
		"https://api.go.example/bytes",
		PhaseRequest,
		[]byte{'f', 'r', 'o', 'm', '=', '1', 0, 'x'},
	)
	if err != nil {
		t.Fatal(err)
	}
	if string(chainBody) != "from=1\x00x" {
		t.Fatalf("chain body = %q", chainBody)
	}
	if chain.Rewritten || chain.Truncated || len(chain.Rewrites) != 1 {
		t.Fatalf("unexpected body chain: %+v", chain)
	}
	if chain.Rewrites[0].Message != "binary body passthrough" {
		t.Fatalf("chain rewrite message = %q", chain.Rewrites[0].Message)
	}
}
