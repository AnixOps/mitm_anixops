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
	if Version() != "0.42.0" {
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
