package main

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: ${SRCDIR}/../../build/libmitm_anixops.a
#cgo windows LDFLAGS: -lregex
#include "mitm_anixops.h"
#include <stdlib.h>
*/
import "C"

import (
	"bufio"
	"bytes"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/json"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"math/big"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"strings"
	"sync"
	"time"
	"unsafe"
)

const (
	rewriteNone        = C.ANIXOPS_REWRITE_NONE
	rewriteRedirect302 = C.ANIXOPS_REWRITE_REDIRECT_302
	rewriteRedirect307 = C.ANIXOPS_REWRITE_REDIRECT_307
	mitmIntercept      = C.ANIXOPS_MITM_INTERCEPT
	scriptNone         = C.ANIXOPS_SCRIPT_NONE
	phaseRequest       = C.ANIXOPS_PHASE_REQUEST
	phaseResponse      = C.ANIXOPS_PHASE_RESPONSE
)

type engine struct {
	ptr *C.anixops_engine_t
}

type caMaterial struct {
	cert    *x509.Certificate
	key     *rsa.PrivateKey
	certPEM []byte
}

type certCache struct {
	ca    caMaterial
	mutex sync.Mutex
	certs map[string]tls.Certificate
}

type proxyServer struct {
	engine         engine
	certs          *certCache
	httpClient     *http.Client
	mihomoProxy    *url.URL
	scriptRunner   string
	scriptPathURL  string
	scriptPathFile string
	tempDir        string
}

type scriptMatch struct {
	kind         C.anixops_script_kind_t
	requiresBody int
	scriptPath   string
	tag          string
	argument     string
}

type scriptDone struct {
	Status  interface{}       `json:"status"`
	URL     string            `json:"url"`
	Headers map[string]string `json:"headers"`
	Body    *string           `json:"body"`
}

func main() {
	listen := flag.String("listen", "127.0.0.1:19080", "shim proxy listen address")
	originListen := flag.String("origin-listen", "", "optional local HTTPS origin listen address")
	mihomoProxyRaw := flag.String("mihomo-proxy", "http://127.0.0.1:19091", "mihomo HTTP proxy URL")
	configPath := flag.String("config", "", "AnixOps-style config fixture")
	caCertPath := flag.String("ca-cert", "", "path to write test CA certificate")
	caKeyPath := flag.String("ca-key", "", "path to write test CA key")
	scriptRunner := flag.String("script-runner", "", "optional Node.js AnixOps script runner path")
	scriptPathURL := flag.String("script-path-url", "", "script URL to map to a local file")
	scriptPathFile := flag.String("script-path-file", "", "local script file for script-path-url")
	flag.Parse()

	if *configPath == "" || *caCertPath == "" || *caKeyPath == "" {
		log.Fatal("--config, --ca-cert, and --ca-key are required")
	}

	eng, err := newEngine(*configPath)
	if err != nil {
		log.Fatal(err)
	}
	defer eng.close()

	ca, err := newCA()
	if err != nil {
		log.Fatal(err)
	}
	if err := os.WriteFile(*caCertPath, ca.certPEM, 0644); err != nil {
		log.Fatal(err)
	}
	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(ca.key)})
	if err := os.WriteFile(*caKeyPath, keyPEM, 0600); err != nil {
		log.Fatal(err)
	}

	cache := &certCache{ca: ca, certs: make(map[string]tls.Certificate)}
	if *originListen != "" {
		if err := startOrigin(*originListen, cache); err != nil {
			log.Fatal(err)
		}
	}

	mihomoProxy, err := url.Parse(*mihomoProxyRaw)
	if err != nil {
		log.Fatal(err)
	}
	ps := &proxyServer{
		engine:         eng,
		certs:          cache,
		mihomoProxy:    mihomoProxy,
		scriptRunner:   *scriptRunner,
		scriptPathURL:  *scriptPathURL,
		scriptPathFile: *scriptPathFile,
		tempDir:        os.TempDir(),
		httpClient: &http.Client{Transport: &http.Transport{
			Proxy: http.ProxyURL(mihomoProxy),
			TLSClientConfig: &tls.Config{
				InsecureSkipVerify: true,
				NextProtos:         []string{"http/1.1"},
			},
		}},
	}

	server := &http.Server{
		Handler:           ps,
		ReadHeaderTimeout: 5 * time.Second,
	}
	ln, err := net.Listen("tcp", *listen)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("READY listen=%s origin=%s mihomo=%s ca=%s", *listen, originLabel(*originListen), *mihomoProxyRaw, *caCertPath)
	if err := server.Serve(ln); !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
}

func newEngine(configPath string) (engine, error) {
	data, err := os.ReadFile(configPath)
	if err != nil {
		return engine{}, err
	}
	ptr := C.anixops_engine_new()
	if ptr == nil {
		return engine{}, errors.New("anixops_engine_new returned nil")
	}
	cconf := C.CString(string(data))
	defer C.free(unsafe.Pointer(cconf))
	if rc := C.anixops_engine_load_config(ptr, cconf); rc != C.ANIXOPS_OK {
		C.anixops_engine_free(ptr)
		return engine{}, fmt.Errorf("anixops_engine_load_config failed: %d", int(rc))
	}
	C.anixops_engine_set_mitm_enabled(ptr, 1)
	C.anixops_engine_set_cert_state(ptr, C.ANIXOPS_CERT_TRUSTED)
	return engine{ptr: ptr}, nil
}

func (e engine) close() {
	if e.ptr != nil {
		C.anixops_engine_free(e.ptr)
	}
}

func (e engine) rewrite(rawURL string) (action C.anixops_rewrite_action_t, status int, value string, err error) {
	curl := C.CString(rawURL)
	defer C.free(unsafe.Pointer(curl))
	var result C.anixops_rewrite_result_t
	rc := C.anixops_rewrite_evaluate_url(e.ptr, curl, C.ANIXOPS_PHASE_REQUEST, &result)
	if rc != C.ANIXOPS_OK {
		return rewriteNone, 0, "", fmt.Errorf("anixops_rewrite_evaluate_url failed: %d", int(rc))
	}
	return result.action, int(result.status_code), cStringFromArray(unsafe.Pointer(&result.value[0])), nil
}

func (e engine) script(rawURL string, phase C.anixops_phase_t) (scriptMatch, error) {
	curl := C.CString(rawURL)
	defer C.free(unsafe.Pointer(curl))
	var result C.anixops_script_result_t
	rc := C.anixops_script_evaluate_url(e.ptr, curl, phase, &result)
	if rc != C.ANIXOPS_OK {
		return scriptMatch{}, fmt.Errorf("anixops_script_evaluate_url failed: %d", int(rc))
	}
	return scriptMatch{
		kind:         result.kind,
		requiresBody: int(result.requires_body),
		scriptPath:   cStringFromArray(unsafe.Pointer(&result.script_path[0])),
		tag:          cStringFromArray(unsafe.Pointer(&result.tag[0])),
		argument:     cStringFromArray(unsafe.Pointer(&result.argument[0])),
	}, nil
}

func (e engine) shouldMITM(host string) (bool, error) {
	chost := C.CString(host)
	defer C.free(unsafe.Pointer(chost))
	var decision C.anixops_mitm_decision_t
	rc := C.anixops_mitm_evaluate(e.ptr, chost, 0, &decision)
	if rc != C.ANIXOPS_OK {
		return false, fmt.Errorf("anixops_mitm_evaluate failed: %d", int(rc))
	}
	return decision.decision == mitmIntercept, nil
}

func cStringFromArray(ptr unsafe.Pointer) string {
	return C.GoString((*C.char)(ptr))
}

func (ps *proxyServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodConnect {
		ps.handleConnect(w, r)
		return
	}
	ps.handlePlainHTTP(w, r)
}

func (ps *proxyServer) handlePlainHTTP(w http.ResponseWriter, r *http.Request) {
	rawURL := r.URL.String()
	if !r.URL.IsAbs() {
		rawURL = "http://" + r.Host + r.URL.RequestURI()
	}
	action, status, value, err := ps.engine.rewrite(rawURL)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	if writeRewrite(w, action, status, value) {
		return
	}
	rawURL, host, header, body, err := ps.applyRequestScript(rawURL, r.Method, r.Host, r.Header, r.Body)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	header = ps.prepareUpstreamHeader(rawURL, header)
	resp, err := ps.forward(r.Context().Done(), r.Method, rawURL, host, header, body)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	defer resp.Body.Close()
	if err := ps.applyResponseScript(rawURL, r.Method, header, resp); err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	copyResponse(w, resp)
}

func (ps *proxyServer) handleConnect(w http.ResponseWriter, r *http.Request) {
	hostOnly := stripPort(r.Host)
	intercept, err := ps.engine.shouldMITM(hostOnly)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}

	hijacker, ok := w.(http.Hijacker)
	if !ok {
		http.Error(w, "hijacking not supported", http.StatusInternalServerError)
		return
	}
	clientConn, _, err := hijacker.Hijack()
	if err != nil {
		return
	}
	defer clientConn.Close()
	if !intercept {
		ps.tunnelConnect(clientConn, r.Host)
		return
	}
	if _, err := io.WriteString(clientConn, "HTTP/1.1 200 Connection Established\r\n\r\n"); err != nil {
		return
	}

	cert, err := ps.certs.certForHost(hostOnly)
	if err != nil {
		return
	}
	tlsConn := tls.Server(clientConn, &tls.Config{
		Certificates: []tls.Certificate{cert},
		NextProtos:   []string{"http/1.1"},
	})
	if err := tlsConn.Handshake(); err != nil {
		return
	}
	defer tlsConn.Close()

	reader := bufio.NewReader(tlsConn)
	for {
		req, err := http.ReadRequest(reader)
		if err != nil {
			return
		}
		ps.handleMITMRequest(tlsConn, r.Host, req)
	}
}

func (ps *proxyServer) handleMITMRequest(conn net.Conn, connectHost string, req *http.Request) {
	defer req.Body.Close()
	host := req.Host
	if host == "" {
		host = connectHost
	}
	rawURL := "https://" + host + req.URL.RequestURI()
	action, status, value, err := ps.engine.rewrite(rawURL)
	if err != nil {
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	if writeRewriteToConn(conn, action, status, value) {
		return
	}

	rawURL, host, header, body, err := ps.applyRequestScript(rawURL, req.Method, host, req.Header, req.Body)
	if err != nil {
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	header = ps.prepareUpstreamHeader(rawURL, header)
	resp, err := ps.forward(nil, req.Method, rawURL, host, header, body)
	if err != nil {
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	defer resp.Body.Close()
	if err := ps.applyResponseScript(rawURL, req.Method, header, resp); err != nil {
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	if err := resp.Write(conn); err != nil {
		return
	}
}

func (ps *proxyServer) applyRequestScript(
	rawURL string,
	method string,
	host string,
	header http.Header,
	body io.Reader,
) (string, string, http.Header, io.Reader, error) {
	scriptURL := scriptDispatchURL(rawURL)
	match, err := ps.engine.script(scriptURL, phaseRequest)
	if err != nil {
		return "", "", nil, nil, err
	}

	var bodyBytes []byte
	if body != nil {
		bodyBytes, err = io.ReadAll(body)
		if err != nil {
			return "", "", nil, nil, err
		}
	}
	nextHeader := cloneHeader(header)
	if match.kind == scriptNone || ps.scriptRunner == "" {
		return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
	}

	done, err := ps.runScript("request", match, scriptURL, method, nextHeader, 0, nil, bodyBytes)
	if err != nil {
		return "", "", nil, nil, err
	}
	if done.URL != "" {
		rawURL = done.URL
		if parsed, parseErr := url.Parse(rawURL); parseErr == nil && parsed.Host != "" {
			host = parsed.Host
		}
	}
	if done.Headers != nil {
		nextHeader = make(http.Header)
		for key, value := range done.Headers {
			nextHeader.Set(key, value)
		}
	}
	if done.Body != nil {
		bodyBytes = []byte(*done.Body)
	}
	return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
}

func (ps *proxyServer) prepareUpstreamHeader(rawURL string, header http.Header) http.Header {
	nextHeader := cloneHeader(header)
	if ps.scriptRunner == "" {
		return nextHeader
	}
	match, err := ps.engine.script(scriptDispatchURL(rawURL), phaseResponse)
	if err != nil || match.kind == scriptNone || match.requiresBody == 0 {
		return nextHeader
	}
	nextHeader.Set("Accept-Encoding", "identity")
	return nextHeader
}

func (ps *proxyServer) applyResponseScript(rawURL string, method string, requestHeader http.Header, resp *http.Response) error {
	scriptURL := scriptDispatchURL(rawURL)
	match, err := ps.engine.script(scriptURL, phaseResponse)
	if err != nil {
		return err
	}
	if match.kind == scriptNone {
		return nil
	}
	if ps.scriptRunner == "" {
		return nil
	}

	bodyBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	_ = resp.Body.Close()
	done, err := ps.runScript(
		"response",
		match,
		scriptURL,
		method,
		requestHeader,
		resp.StatusCode,
		resp.Header,
		bodyBytes)
	if err != nil {
		return err
	}
	if done.Headers != nil {
		resp.Header = make(http.Header)
		for key, value := range done.Headers {
			resp.Header.Set(key, value)
		}
	}
	if done.Body != nil {
		resp.Body = io.NopCloser(strings.NewReader(*done.Body))
		resp.ContentLength = int64(len(*done.Body))
		resp.Header.Del("Content-Encoding")
		resp.Header.Set("Content-Length", fmt.Sprintf("%d", len(*done.Body)))
	}
	if status, ok := numericStatus(done.Status); ok {
		resp.StatusCode = status
		resp.Status = fmt.Sprintf("%d %s", status, http.StatusText(status))
	}
	return nil
}

func (ps *proxyServer) runScript(
	phase string,
	match scriptMatch,
	scriptURL string,
	method string,
	requestHeader http.Header,
	responseStatus int,
	responseHeader http.Header,
	bodyBytes []byte,
) (scriptDone, error) {
	scriptFile := ps.scriptPathFile
	if match.scriptPath != ps.scriptPathURL {
		return scriptDone{}, fmt.Errorf("no local script mapping for %s", match.scriptPath)
	}
	if scriptFile == "" {
		return scriptDone{}, fmt.Errorf("empty local script mapping for %s", match.scriptPath)
	}

	bodyFile, err := os.CreateTemp(ps.tempDir, "anixops-script-body-*.json")
	if err != nil {
		return scriptDone{}, err
	}
	bodyPath := bodyFile.Name()
	_, writeErr := bodyFile.Write(bodyBytes)
	closeErr := bodyFile.Close()
	defer os.Remove(bodyPath)
	if writeErr != nil {
		return scriptDone{}, writeErr
	}
	if closeErr != nil {
		return scriptDone{}, closeErr
	}

	requestHeaderJSON, err := json.Marshal(flattenHeaders(requestHeader))
	if err != nil {
		return scriptDone{}, err
	}

	args := []string{
		ps.scriptRunner,
		"--script", scriptFile,
		"--url", scriptURL,
		"--argument", match.argument,
		"--body", bodyPath,
		"--phase", phase,
		"--method", method,
		"--requestHeaders", string(requestHeaderJSON),
	}
	if phase == "response" {
		headerJSON, err := json.Marshal(flattenHeaders(responseHeader))
		if err != nil {
			return scriptDone{}, err
		}
		args = append(args,
			"--status", fmt.Sprintf("%d", responseStatus),
			"--responseHeaders", string(headerJSON))
	}
	cmd := exec.Command("node", args...)
	output, err := cmd.Output()
	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			return scriptDone{}, fmt.Errorf("script runner failed: %s", strings.TrimSpace(string(exitErr.Stderr)))
		}
		return scriptDone{}, err
	}
	var done scriptDone
	if err := json.Unmarshal(output, &done); err != nil {
		return scriptDone{}, err
	}
	return done, nil
}

func (ps *proxyServer) tunnelConnect(clientConn net.Conn, target string) {
	upstreamAddr := proxyTCPAddress(ps.mihomoProxy)
	upstreamConn, err := net.DialTimeout("tcp", upstreamAddr, 10*time.Second)
	if err != nil {
		writeRawError(clientConn, http.StatusBadGateway, err.Error())
		return
	}
	defer upstreamConn.Close()

	if _, err := fmt.Fprintf(
		upstreamConn,
		"CONNECT %s HTTP/1.1\r\nHost: %s\r\nProxy-Connection: keep-alive\r\n\r\n",
		target,
		target); err != nil {
		return
	}
	upstreamReader := bufio.NewReader(upstreamConn)
	resp, err := http.ReadResponse(upstreamReader, &http.Request{Method: http.MethodConnect})
	if err != nil {
		writeRawError(clientConn, http.StatusBadGateway, err.Error())
		return
	}
	if resp.Body != nil {
		_ = resp.Body.Close()
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		writeRawError(clientConn, resp.StatusCode, resp.Status)
		return
	}
	if _, err := io.WriteString(clientConn, "HTTP/1.1 200 Connection Established\r\n\r\n"); err != nil {
		return
	}

	done := make(chan struct{}, 2)
	go func() {
		_, _ = io.Copy(upstreamConn, clientConn)
		done <- struct{}{}
	}()
	go func() {
		_, _ = io.Copy(clientConn, upstreamReader)
		done <- struct{}{}
	}()
	<-done
}

func scriptDispatchURL(rawURL string) string {
	parsed, err := url.Parse(rawURL)
	if err != nil || parsed.Host == "" {
		return rawURL
	}
	host := parsed.Hostname()
	if host == "" {
		return rawURL
	}
	parsed.Host = host
	return parsed.String()
}

func (ps *proxyServer) forward(done <-chan struct{}, method, rawURL, host string, header http.Header, body io.Reader) (*http.Response, error) {
	var payload []byte
	var err error
	if body != nil {
		payload, err = io.ReadAll(body)
		if err != nil {
			return nil, err
		}
	}
	req, err := http.NewRequest(method, rawURL, bytes.NewReader(payload))
	if err != nil {
		return nil, err
	}
	req.Host = host
	req.Header = cloneHeader(header)
	req.RequestURI = ""
	if done != nil {
		go func() {
			<-done
			if req.Body != nil {
				req.Body.Close()
			}
		}()
	}
	return ps.httpClient.Do(req)
}

func writeRewrite(w http.ResponseWriter, action C.anixops_rewrite_action_t, status int, value string) bool {
	switch action {
	case rewriteRedirect302, rewriteRedirect307:
		if status == 0 {
			status = http.StatusFound
		}
		w.Header().Set("Location", value)
		w.WriteHeader(status)
		return true
	case rewriteNone:
		return false
	default:
		if status == 0 {
			status = http.StatusForbidden
		}
		w.WriteHeader(status)
		return true
	}
}

func writeRewriteToConn(conn net.Conn, action C.anixops_rewrite_action_t, status int, value string) bool {
	switch action {
	case rewriteRedirect302, rewriteRedirect307:
		if status == 0 {
			status = http.StatusFound
		}
		resp := &http.Response{
			StatusCode:    status,
			Status:        fmt.Sprintf("%d %s", status, http.StatusText(status)),
			ProtoMajor:    1,
			ProtoMinor:    1,
			Header:        make(http.Header),
			Body:          io.NopCloser(strings.NewReader("")),
			Close:         true,
			ContentLength: 0,
		}
		resp.Header.Set("Location", value)
		_ = resp.Write(conn)
		return true
	case rewriteNone:
		return false
	default:
		if status == 0 {
			status = http.StatusForbidden
		}
		writeRawError(conn, status, http.StatusText(status))
		return true
	}
}

func writeRawError(conn net.Conn, status int, message string) {
	body := message + "\n"
	_, _ = fmt.Fprintf(conn, "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		status, http.StatusText(status), len(body), body)
}

func copyResponse(w http.ResponseWriter, resp *http.Response) {
	for key, values := range resp.Header {
		for _, value := range values {
			w.Header().Add(key, value)
		}
	}
	w.WriteHeader(resp.StatusCode)
	_, _ = io.Copy(w, resp.Body)
}

func cloneHeader(in http.Header) http.Header {
	out := make(http.Header, len(in))
	for key, values := range in {
		copied := make([]string, len(values))
		copy(copied, values)
		out[key] = copied
	}
	return out
}

func flattenHeaders(in http.Header) map[string]string {
	out := make(map[string]string, len(in))
	for key, values := range in {
		if len(values) == 0 {
			continue
		}
		out[key] = values[0]
	}
	return out
}

func numericStatus(value interface{}) (int, bool) {
	switch typed := value.(type) {
	case float64:
		return int(typed), typed >= 100 && typed <= 999
	case string:
		var status int
		if _, err := fmt.Sscanf(typed, "HTTP/1.1 %d", &status); err == nil {
			return status, true
		}
		if _, err := fmt.Sscanf(typed, "%d", &status); err == nil {
			return status, true
		}
	}
	return 0, false
}

func stripPort(hostport string) string {
	host, _, err := net.SplitHostPort(hostport)
	if err == nil {
		return host
	}
	return strings.Trim(hostport, "[]")
}

func proxyTCPAddress(proxyURL *url.URL) string {
	if proxyURL == nil {
		return "127.0.0.1:7890"
	}
	host := proxyURL.Host
	if host == "" {
		return "127.0.0.1:7890"
	}
	if _, _, err := net.SplitHostPort(host); err == nil {
		return host
	}
	switch strings.ToLower(proxyURL.Scheme) {
	case "https":
		return net.JoinHostPort(host, "443")
	default:
		return net.JoinHostPort(host, "80")
	}
}

func originLabel(addr string) string {
	if addr == "" {
		return "disabled"
	}
	return addr
}

func startOrigin(addr string, cache *certCache) error {
	cert, err := cache.certForHost("google.com")
	if err != nil {
		return err
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if stripPort(r.Host) == "www.bilibili.com" && r.URL.Path == "/" {
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			io.WriteString(w, "<!doctype html><html><head><title>origin title</title></head><body><main><a class=\"bili-video-card__info--tit\" href=\"/video/BV1\">origin title</a><img class=\"bili-video-card__cover\" src=\"/cover.jpg\" alt=\"cover\"></main></body></html>")
			return
		}
		if strings.HasPrefix(r.URL.Path, "/contract/request-response") {
			body, _ := io.ReadAll(r.Body)
			w.Header().Set("Content-Type", "application/json")
			_ = json.NewEncoder(w).Encode(map[string]interface{}{
				"code":          0,
				"requestScript": r.Header.Get("X-AnixOps-Request-Script"),
				"body":          string(body),
			})
			return
		}
		if strings.HasPrefix(r.URL.Path, "/x/resource/show/tab/v2") ||
			strings.HasPrefix(r.URL.Path, "/x/v2/account/mine") {
			w.Header().Set("Content-Type", "application/json")
			io.WriteString(w, "{\"code\":0,\"message\":\"0\",\"data\":{}}\n")
			return
		}
		if strings.HasPrefix(r.URL.Path, "/x/v2/region/index") ||
			strings.HasPrefix(r.URL.Path, "/x/v2/channel/region/list") {
			w.Header().Set("Content-Type", "application/json")
			io.WriteString(w, "{\"code\":0,\"message\":\"0\",\"data\":[]}\n")
			return
		}
		w.Header().Set("Content-Type", "text/plain")
		fmt.Fprintf(w, "origin-ok host=%s path=%s\n", r.Host, r.URL.Path)
	})
	server := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
		TLSConfig: &tls.Config{
			Certificates: []tls.Certificate{cert},
			NextProtos:   []string{"http/1.1"},
		},
	}
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	go func() {
		_ = server.ServeTLS(ln, "", "")
	}()
	return nil
}

func newCA() (caMaterial, error) {
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return caMaterial{}, err
	}
	template := &x509.Certificate{
		SerialNumber:          big.NewInt(1),
		Subject:               pkix.Name{CommonName: "mitm_anixops e2e CA"},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(24 * time.Hour),
		KeyUsage:              x509.KeyUsageCertSign | x509.KeyUsageCRLSign,
		BasicConstraintsValid: true,
		IsCA:                  true,
	}
	der, err := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if err != nil {
		return caMaterial{}, err
	}
	cert, err := x509.ParseCertificate(der)
	if err != nil {
		return caMaterial{}, err
	}
	return caMaterial{
		cert:    cert,
		key:     key,
		certPEM: pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der}),
	}, nil
}

func (c *certCache) certForHost(host string) (tls.Certificate, error) {
	host = strings.ToLower(strings.TrimSpace(host))
	c.mutex.Lock()
	defer c.mutex.Unlock()
	if cert, ok := c.certs[host]; ok {
		return cert, nil
	}
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return tls.Certificate{}, err
	}
	serial, err := rand.Int(rand.Reader, new(big.Int).Lsh(big.NewInt(1), 128))
	if err != nil {
		return tls.Certificate{}, err
	}
	template := &x509.Certificate{
		SerialNumber: serial,
		Subject:      pkix.Name{CommonName: host},
		NotBefore:    time.Now().Add(-time.Hour),
		NotAfter:     time.Now().Add(24 * time.Hour),
		KeyUsage:     x509.KeyUsageDigitalSignature | x509.KeyUsageKeyEncipherment,
		ExtKeyUsage:  []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		DNSNames:     []string{host},
	}
	if ip := net.ParseIP(host); ip != nil {
		template.IPAddresses = []net.IP{ip}
		template.DNSNames = nil
	}
	der, err := x509.CreateCertificate(rand.Reader, template, c.ca.cert, &key.PublicKey, c.ca.key)
	if err != nil {
		return tls.Certificate{}, err
	}
	certPEM := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der})
	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
	cert, err := tls.X509KeyPair(certPEM, keyPEM)
	if err != nil {
		return tls.Certificate{}, err
	}
	c.certs[host] = cert
	return cert, nil
}
