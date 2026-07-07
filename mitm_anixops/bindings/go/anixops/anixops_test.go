package anixops

import "testing"

const fixtureConfig = `
[Argument]
Mode = select,go

[Rewrite]
^http:\/\/old\.go\.example\/(.*) https://api.go.example/$1 302
^https:\/\/api\.go\.example\/v1 response-header-add X-Go go-plan
^https:\/\/api\.go\.example\/v1 response-body-replace-regex from to

[Script]
http-response ^https:\/\/api\.go\.example\/v1 requires-body=1, script-path=https://scripts.example/go-response.js, tag=go.response, argument=[{Mode}]
`

func TestGoBindingEvaluatesPolicy(t *testing.T) {
	if Version() != "0.44.0" {
		t.Fatalf("Version() = %q", Version())
	}
	engine, err := NewEngine()
	if err != nil {
		t.Fatal(err)
	}
	defer engine.Close()

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

	script, err := engine.EvaluateScript("https://api.go.example/v1", PhaseResponse)
	if err != nil {
		t.Fatal(err)
	}
	if script.Kind != ScriptHTTPResponse || !script.RequiresBody {
		t.Fatalf("unexpected script result: %+v", script)
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
