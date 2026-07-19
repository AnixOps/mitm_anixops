package main

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"
)

type countingReader struct {
	reader io.Reader
	read   int
}

func (r *countingReader) Read(buffer []byte) (int, error) {
	n, err := r.reader.Read(buffer)
	r.read += n
	return n, err
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(req *http.Request) (*http.Response, error) {
	return f(req)
}

type trailerPublishingReader struct {
	reader    io.Reader
	trailer   http.Header
	published bool
}

func (r *trailerPublishingReader) Read(buffer []byte) (int, error) {
	n, err := r.reader.Read(buffer)
	if err == io.EOF && !r.published {
		r.trailer.Set("X-AnixOps-Relay-Trailer", "preserved")
		r.published = true
	}
	return n, err
}

func TestEngineWithCoreHoldsMutexForPolicyCoreCall(t *testing.T) {
	core := &engine{}
	entered := make(chan struct{})
	release := make(chan struct{})
	done := make(chan struct{})
	var releaseOnce sync.Once
	releaseCore := func() {
		releaseOnce.Do(func() {
			close(release)
		})
	}
	defer releaseCore()

	go func() {
		core.withCore(func() {
			close(entered)
			<-release
		})
		close(done)
	}()

	select {
	case <-entered:
	case <-time.After(time.Second):
		t.Fatal("policy-core call did not enter the serialized region")
	}

	if core.mu.TryLock() {
		core.mu.Unlock()
		t.Fatal("policy-core mutex was not held for the complete call")
	}

	releaseCore()
	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("policy-core call did not leave the serialized region")
	}
}

func TestReadBodyForMutationRelaysRawOverflow(t *testing.T) {
	original := []byte("0123456789")
	source := &countingReader{reader: bytes.NewReader(original)}

	buffered, relay, overflow, err := readBodyForMutation(source, 4)
	if err != nil {
		t.Fatalf("read body for mutation: %v", err)
	}
	if !overflow {
		t.Fatal("expected raw body overflow")
	}
	if buffered != nil {
		t.Fatalf("overflow unexpectedly returned buffered data: %q", buffered)
	}
	if source.read != 5 {
		t.Fatalf("overflow reader consumed %d bytes before relay, want 5", source.read)
	}

	got, err := io.ReadAll(relay)
	if err != nil {
		t.Fatalf("read raw relay: %v", err)
	}
	if !bytes.Equal(got, original) {
		t.Fatalf("raw relay = %q, want %q", got, original)
	}
}

func TestReadBodyForMutationBuffersWithinLimit(t *testing.T) {
	original := []byte("0123")

	buffered, relay, overflow, err := readBodyForMutation(bytes.NewReader(original), len(original))
	if err != nil {
		t.Fatalf("read body for mutation: %v", err)
	}
	if overflow {
		t.Fatal("body at the limit unexpectedly overflowed")
	}
	if relay != nil {
		t.Fatal("body at the limit unexpectedly returned a raw relay")
	}
	if !bytes.Equal(buffered, original) {
		t.Fatalf("buffered body = %q, want %q", buffered, original)
	}
}

func TestNewUpstreamHTTPClientDisablesImplicitCompression(t *testing.T) {
	upstream, err := url.Parse("http://127.0.0.1:7890")
	if err != nil {
		t.Fatalf("parse upstream URL: %v", err)
	}

	client := newUpstreamHTTPClient(upstream)
	transport, ok := client.Transport.(*http.Transport)
	if !ok {
		t.Fatalf("transport type = %T, want *http.Transport", client.Transport)
	}
	if !transport.DisableCompression {
		t.Fatal("upstream transport must not synthesize Accept-Encoding or transparently decode responses")
	}
}

func TestForwardPreservesRawRelayFramingAndTrailers(t *testing.T) {
	trailer := make(http.Header)
	trailer["X-AnixOps-Relay-Trailer"] = nil
	source := &trailerPublishingReader{
		reader:  strings.NewReader("relay payload"),
		trailer: trailer,
	}

	var observed *http.Request
	var forwardedBody []byte
	server := &proxyServer{
		httpClient: &http.Client{Transport: roundTripFunc(func(req *http.Request) (*http.Response, error) {
			observed = req
			var err error
			forwardedBody, err = io.ReadAll(req.Body)
			if err != nil {
				return nil, err
			}
			return &http.Response{
				StatusCode: http.StatusOK,
				Body:       io.NopCloser(strings.NewReader("")),
				Header:     make(http.Header),
			}, nil
		})},
	}

	resp, err := server.forward(
		nil,
		http.MethodPost,
		"http://origin.example.test/raw-relay",
		"origin.example.test",
		http.Header{"Content-Type": {"application/octet-stream"}},
		source,
		&forwardRelayMetadata{
			contentLength:     -1,
			transferEncoding: []string{"chunked"},
			trailer:           trailer,
		},
	)
	if err != nil {
		t.Fatalf("forward raw relay: %v", err)
	}
	defer resp.Body.Close()

	if string(forwardedBody) != "relay payload" {
		t.Fatalf("forwarded body = %q", forwardedBody)
	}
	if observed.ContentLength != -1 {
		t.Fatalf("forwarded content length = %d, want -1", observed.ContentLength)
	}
	if len(observed.TransferEncoding) != 1 || observed.TransferEncoding[0] != "chunked" {
		t.Fatalf("forwarded transfer encoding = %v, want [chunked]", observed.TransferEncoding)
	}
	if got := observed.Trailer.Get("X-AnixOps-Relay-Trailer"); got != "preserved" {
		t.Fatalf("forwarded trailer = %q, want preserved", got)
	}
}

func TestForwardPreservesBufferedRequestFramingAndTrailers(t *testing.T) {
	type observedRequest struct {
		body             string
		contentLength    int64
		transferEncoding []string
		trailer          string
	}
	received := make(chan observedRequest, 1)
	origin := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, req *http.Request) {
		body, err := io.ReadAll(req.Body)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		received <- observedRequest{
			body:             string(body),
			contentLength:    req.ContentLength,
			transferEncoding: append([]string(nil), req.TransferEncoding...),
			trailer:          req.Trailer.Get("X-AnixOps-Request-Trailer"),
		}
		w.WriteHeader(http.StatusOK)
	}))
	defer origin.Close()
	originURL, err := url.Parse(origin.URL)
	if err != nil {
		t.Fatalf("parse origin URL: %v", err)
	}

	trailer := http.Header{"X-AnixOps-Request-Trailer": {"preserved"}}
	inbound := &http.Request{
		ContentLength:     -1,
		TransferEncoding: []string{"chunked"},
		Trailer:           trailer,
	}

	server := &proxyServer{httpClient: origin.Client()}

	resp, err := server.forward(
		nil,
		http.MethodPost,
		origin.URL+"/buffered",
		originURL.Host,
		http.Header{"Content-Type": {"application/octet-stream"}},
		bytes.NewReader([]byte("buffered payload")),
		snapshotRequestRelayMetadata(inbound),
	)
	if err != nil {
		t.Fatalf("forward buffered request: %v", err)
	}
	defer resp.Body.Close()

	select {
	case observed := <-received:
		if observed.body != "buffered payload" {
			t.Fatalf("forwarded body = %q", observed.body)
		}
		if observed.contentLength != -1 {
			t.Fatalf("forwarded content length = %d, want -1", observed.contentLength)
		}
		if len(observed.transferEncoding) != 1 || observed.transferEncoding[0] != "chunked" {
			t.Fatalf("forwarded transfer encoding = %v, want [chunked]", observed.transferEncoding)
		}
		if observed.trailer != "preserved" {
			t.Fatalf("forwarded trailer = %q, want preserved", observed.trailer)
		}
	case <-time.After(time.Second):
		t.Fatal("origin did not receive buffered request")
	}
}

func TestForwardPreservesKnownEmptyRequestBody(t *testing.T) {
	inbound := &http.Request{ContentLength: 0}
	var observed *http.Request
	server := &proxyServer{
		httpClient: &http.Client{Transport: roundTripFunc(func(req *http.Request) (*http.Response, error) {
			observed = req
			return &http.Response{
				StatusCode: http.StatusOK,
				Body:       io.NopCloser(strings.NewReader("")),
				Header:     make(http.Header),
			}, nil
		})},
	}

	resp, err := server.forward(
		nil,
		http.MethodPost,
		"http://origin.example.test/empty",
		"origin.example.test",
		make(http.Header),
		io.NopCloser(strings.NewReader("")),
		snapshotRequestRelayMetadata(inbound),
	)
	if err != nil {
		t.Fatalf("forward known-empty request: %v", err)
	}
	defer resp.Body.Close()

	if observed.ContentLength != 0 {
		t.Fatalf("forwarded content length = %d, want 0", observed.ContentLength)
	}
	if len(observed.TransferEncoding) != 0 {
		t.Fatalf("forwarded transfer encoding = %v, want none", observed.TransferEncoding)
	}
	if observed.Body != http.NoBody {
		t.Fatalf("forwarded body = %T, want http.NoBody", observed.Body)
	}
}

func TestForwardWritesRawRelayTrailers(t *testing.T) {
	type observedRequest struct {
		body             string
		transferEncoding []string
		trailer          string
	}
	received := make(chan observedRequest, 1)
	origin := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, req *http.Request) {
		body, err := io.ReadAll(req.Body)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		received <- observedRequest{
			body:             string(body),
			transferEncoding: append([]string(nil), req.TransferEncoding...),
			trailer:          req.Trailer.Get("X-AnixOps-Relay-Trailer"),
		}
		w.WriteHeader(http.StatusOK)
	}))
	defer origin.Close()
	originURL, err := url.Parse(origin.URL)
	if err != nil {
		t.Fatalf("parse origin URL: %v", err)
	}

	trailer := make(http.Header)
	trailer["X-AnixOps-Relay-Trailer"] = nil
	server := &proxyServer{httpClient: origin.Client()}
	resp, err := server.forward(
		nil,
		http.MethodPost,
		origin.URL+"/raw-relay",
		originURL.Host,
		http.Header{"Content-Type": {"application/octet-stream"}},
		&trailerPublishingReader{
			reader:  strings.NewReader("relay payload"),
			trailer: trailer,
		},
		&forwardRelayMetadata{
			contentLength:     -1,
			transferEncoding: []string{"chunked"},
			trailer:           trailer,
		},
	)
	if err != nil {
		t.Fatalf("forward raw relay through transport: %v", err)
	}
	defer resp.Body.Close()

	select {
	case got := <-received:
		if got.body != "relay payload" {
			t.Fatalf("origin body = %q", got.body)
		}
		if len(got.transferEncoding) != 1 || got.transferEncoding[0] != "chunked" {
			t.Fatalf("origin transfer encoding = %v, want [chunked]", got.transferEncoding)
		}
		if got.trailer != "preserved" {
			t.Fatalf("origin trailer = %q, want preserved", got.trailer)
		}
	case <-time.After(time.Second):
		t.Fatal("origin did not receive raw relay")
	}
}

func TestCopyResponseForwardsRawRelayTrailers(t *testing.T) {
	trailer := make(http.Header)
	trailer["X-AnixOps-Relay-Trailer"] = nil
	response := &http.Response{
		StatusCode: http.StatusOK,
		Header:     make(http.Header),
		Body: io.NopCloser(&trailerPublishingReader{
			reader:  strings.NewReader("relay response"),
			trailer: trailer,
		}),
		Trailer: trailer,
	}
	recorder := httptest.NewRecorder()

	copyResponse(recorder, response)

	result := recorder.Result()
	defer result.Body.Close()
	body, err := io.ReadAll(result.Body)
	if err != nil {
		t.Fatalf("read copied body: %v", err)
	}
	if string(body) != "relay response" {
		t.Fatalf("copied body = %q", body)
	}
	if got := result.Trailer.Get("X-AnixOps-Relay-Trailer"); got != "preserved" {
		t.Fatalf("copied trailer = %q, want preserved", got)
	}
}

func TestRestoreUnchangedResponseBodyPreservesFramingAndTrailers(t *testing.T) {
	originalHeader := http.Header{
		"Content-Encoding": {"br"},
		"X-Origin":         {"preserved"},
	}
	trailer := http.Header{"X-Origin-Relay-Trailer": {"preserved"}}
	response := &http.Response{
		Header:           cloneHeader(originalHeader),
		Body:             io.NopCloser(strings.NewReader("original body")),
		ContentLength:    -1,
		TransferEncoding: []string{"chunked"},
		Trailer:          trailer,
	}
	nextHeader := cloneHeader(originalHeader)
	nextHeader.Set("X-AnixOps-Static-Response", "retained")

	restoreUnchangedResponseBody(response, []byte("original body"), nextHeader, snapshotResponseFraming(response))

	if response.ContentLength != -1 {
		t.Fatalf("content length = %d, want -1", response.ContentLength)
	}
	if len(response.TransferEncoding) != 1 || response.TransferEncoding[0] != "chunked" {
		t.Fatalf("transfer encoding = %v, want [chunked]", response.TransferEncoding)
	}
	if got := response.Header.Get("Content-Length"); got != "" {
		t.Fatalf("content length header = %q, want absent for chunked response", got)
	}
	if got := response.Trailer.Get("X-Origin-Relay-Trailer"); got != "preserved" {
		t.Fatalf("trailer = %q, want preserved", got)
	}
	if got := response.Header.Get("Content-Encoding"); got != "br" {
		t.Fatalf("content encoding = %q, want br", got)
	}
	if got := response.Header.Get("X-AnixOps-Static-Response"); got != "retained" {
		t.Fatalf("static header = %q, want retained", got)
	}
}

func TestRunScriptEnforcesHostDeadline(t *testing.T) {
	server := newTestScriptRunnerProxy(t, "while (true) {}", "25")
	started := time.Now()

	_, err := server.runScript(
		"response",
		scriptMatch{scriptPath: testScriptRunnerURL, tag: "sync-loop"},
		testScriptRunnerURL,
		http.MethodGet,
		make(http.Header),
		http.StatusOK,
		make(http.Header),
		[]byte("{}"),
	)
	if err == nil || !strings.Contains(err.Error(), "timed out") {
		t.Fatalf("runScript error = %v, want fixed timeout classification", err)
	}
	if elapsed := time.Since(started); elapsed > 2*time.Second {
		t.Fatalf("host deadline elapsed = %s, want under 2s", elapsed)
	}
}

func TestRunScriptBoundsRunnerOutput(t *testing.T) {
	secret := "anixops-runner-stdout-secret"
	server := newTestScriptRunnerProxy(t, fmt.Sprintf("process.stdout.write('%s'.repeat(%d));", secret, 3), "2000")
	server.scriptOutputLimit = 64

	_, err := server.runScript(
		"response",
		scriptMatch{scriptPath: testScriptRunnerURL, tag: "stdout-overflow"},
		testScriptRunnerURL,
		http.MethodGet,
		make(http.Header),
		http.StatusOK,
		make(http.Header),
		[]byte("{}"),
	)
	if err == nil || err.Error() != "script runner output exceeds limit" {
		t.Fatalf("runScript error = %v, want fixed output-limit classification", err)
	}
	if strings.Contains(err.Error(), secret) {
		t.Fatalf("runner error leaked stdout secret: %q", err)
	}
}

func TestRunScriptBoundsRunnerStderrWithoutLeakingIt(t *testing.T) {
	secret := "anixops-runner-stderr-secret"
	server := newTestScriptRunnerProxy(t, fmt.Sprintf("process.stderr.write('%s'.repeat(%d));", secret, 4), "2000")
	server.scriptStderrLimit = len(secret) * 2

	_, err := server.runScript(
		"response",
		scriptMatch{scriptPath: testScriptRunnerURL, tag: "stderr-overflow"},
		testScriptRunnerURL,
		http.MethodGet,
		make(http.Header),
		http.StatusOK,
		make(http.Header),
		[]byte("{}"),
	)
	if err == nil || err.Error() != "script runner output exceeds limit" {
		t.Fatalf("runScript error = %v, want fixed output-limit classification", err)
	}
	if strings.Contains(err.Error(), secret) {
		t.Fatalf("runner error leaked stderr secret: %q", err)
	}
}

func TestRunScriptRejectsOverBudgetDoneHeaders(t *testing.T) {
	runnerSource := "const headers = {}; for (let index = 0; index < 129; index += 1) { headers['X-Test-' + index] = 'v'; } process.stdout.write(JSON.stringify({ headers }));"
	server := newTestScriptRunnerProxy(t, runnerSource, "2000")

	_, err := server.runScript(
		"response",
		scriptMatch{scriptPath: testScriptRunnerURL, tag: "header-overflow"},
		testScriptRunnerURL,
		http.MethodGet,
		make(http.Header),
		http.StatusOK,
		make(http.Header),
		[]byte("{}"),
	)
	if err == nil || err.Error() != "script runner output exceeds limit" {
		t.Fatalf("runScript error = %v, want fixed header-limit classification", err)
	}
}

func TestDecodeScriptDoneAcceptsNodeContractEnvelope(t *testing.T) {
	done, err := decodeScriptDone([]byte(`{
  "status": 201,
  "url": "https://scripts.example.test/next",
  "headers": {"X-AnixOps-Script": "applied"},
  "body": "rewritten",
  "unknown": {"nested": ["ignored"]}
}`))
	if err != nil {
		t.Fatalf("decodeScriptDone: %v", err)
	}
	if done.URL != "https://scripts.example.test/next" {
		t.Fatalf("url = %q", done.URL)
	}
	if status, ok := numericStatus(done.Status); !ok || status != http.StatusCreated {
		t.Fatalf("status = %#v, want %d", done.Status, http.StatusCreated)
	}
	if got := done.Headers["X-AnixOps-Script"]; got != "applied" {
		t.Fatalf("header = %q", got)
	}
	if done.Body == nil || *done.Body != "rewritten" {
		t.Fatalf("body = %v", done.Body)
	}
}

const testScriptRunnerURL = "https://scripts.example.test/test.js"

func newTestScriptRunnerProxy(t *testing.T, runnerSource, timeoutMs string) *proxyServer {
	t.Helper()
	if _, err := exec.LookPath("node"); err != nil {
		t.Skip("node is required for script-runner process tests")
	}
	dir := t.TempDir()
	runnerPath := filepath.Join(dir, "runner.js")
	if err := os.WriteFile(runnerPath, []byte(runnerSource), 0o600); err != nil {
		t.Fatalf("write test runner: %v", err)
	}
	scriptPath := filepath.Join(dir, "script.js")
	if err := os.WriteFile(scriptPath, []byte("// mapped test script\n"), 0o600); err != nil {
		t.Fatalf("write mapped script: %v", err)
	}
	return &proxyServer{
		scriptRunner:    runnerPath,
		scriptPathURL:   testScriptRunnerURL,
		scriptPathFile:  scriptPath,
		scriptTimeoutMs: timeoutMs,
		tempDir:         dir,
	}
}
