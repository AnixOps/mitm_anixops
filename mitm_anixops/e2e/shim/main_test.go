package main

import (
	"bytes"
	"io"
	"net/http"
	"net/http/httptest"
	"net/url"
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
