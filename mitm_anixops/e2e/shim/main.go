package main

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: ${SRCDIR}/../../build/libmitm_anixops.a
#include "mitm_anixops.h"
#include <stdlib.h>
*/
import "C"

import (
	"bufio"
	"bytes"
	"compress/flate"
	"compress/gzip"
	"compress/zlib"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/base64"
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
	"os/signal"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
	"unsafe"
)

const (
	rewriteNone                       = C.ANIXOPS_REWRITE_NONE
	rewriteURLRewrite200              = C.ANIXOPS_REWRITE_URL_REWRITE_200
	rewriteRedirect301                = C.ANIXOPS_REWRITE_REDIRECT_301
	rewriteRedirect302                = C.ANIXOPS_REWRITE_REDIRECT_302
	rewriteRedirect303                = C.ANIXOPS_REWRITE_REDIRECT_303
	rewriteRedirect307                = C.ANIXOPS_REWRITE_REDIRECT_307
	rewriteRedirect308                = C.ANIXOPS_REWRITE_REDIRECT_308
	rewriteReject                     = C.ANIXOPS_REWRITE_REJECT
	rewriteReject200                  = C.ANIXOPS_REWRITE_REJECT_200
	rewriteRejectImg                  = C.ANIXOPS_REWRITE_REJECT_IMG
	rewriteRejectVideo                = C.ANIXOPS_REWRITE_REJECT_VIDEO
	rewriteRejectDict                 = C.ANIXOPS_REWRITE_REJECT_DICT
	rewriteRejectArray                = C.ANIXOPS_REWRITE_REJECT_ARRAY
	rewriteMockRequestBody            = C.ANIXOPS_REWRITE_MOCK_REQUEST_BODY
	rewriteMockResponseBody           = C.ANIXOPS_REWRITE_MOCK_RESPONSE_BODY
	rewriteRequestBodyReplaceRegex    = C.ANIXOPS_REWRITE_REQUEST_BODY_REPLACE_REGEX
	rewriteResponseBodyReplaceRegex   = C.ANIXOPS_REWRITE_RESPONSE_BODY_REPLACE_REGEX
	rewriteRequestBodyJSONReplace     = C.ANIXOPS_REWRITE_REQUEST_BODY_JSON_REPLACE
	rewriteResponseBodyJSONReplace    = C.ANIXOPS_REWRITE_RESPONSE_BODY_JSON_REPLACE
	rewriteRequestBodyJQ              = C.ANIXOPS_REWRITE_REQUEST_BODY_JQ
	rewriteResponseBodyJQ             = C.ANIXOPS_REWRITE_RESPONSE_BODY_JQ
	rewriteHeaderReplace              = C.ANIXOPS_REWRITE_HEADER_REPLACE
	rewriteHeaderAdd                  = C.ANIXOPS_REWRITE_HEADER_ADD
	rewriteHeaderReplaceRegex         = C.ANIXOPS_REWRITE_HEADER_REPLACE_REGEX
	rewriteResponseHeaderDel          = C.ANIXOPS_REWRITE_RESPONSE_HEADER_DEL
	rewriteResponseHeaderReplace      = C.ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE
	rewriteResponseHeaderAdd          = C.ANIXOPS_REWRITE_RESPONSE_HEADER_ADD
	rewriteResponseHeaderReplaceRegex = C.ANIXOPS_REWRITE_RESPONSE_HEADER_REPLACE_REGEX
	rewriteHeaderDel                  = C.ANIXOPS_REWRITE_HEADER_DEL
	mitmIntercept                     = C.ANIXOPS_MITM_INTERCEPT
	scriptNone                        = C.ANIXOPS_SCRIPT_NONE
	phaseRequest                      = C.ANIXOPS_PHASE_REQUEST
	phaseResponse                     = C.ANIXOPS_PHASE_RESPONSE
)

const (
	builtinBilibiliScriptPath = "builtin:anixops/bilibili-homepage-demo"
	defaultListen             = "127.0.0.1:19080"
	defaultExternalUpstream   = "http://127.0.0.1:7890"
	maxBodyRewriteBuffer      = 32 * 1024 * 1024
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
	engine          engine
	certs           *certCache
	httpClient      *http.Client
	upstreamProxy   *url.URL
	scriptRunner    string
	scriptPathURL   string
	scriptPathFile  string
	scriptStore     string
	scriptTimeoutMs string
	tempDir         string
	debug           bool
	requestSeq      uint64
	mutex           sync.Mutex
}

type scriptMatch struct {
	kind         C.anixops_script_kind_t
	requiresBody int
	timeoutMs    uint64
	maxSize      uint64
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
	defaultCACert, defaultCAKey := defaultCAPaths()
	listen := flag.String("listen", defaultListen, "local HTTP/HTTPS proxy listen address")
	originListen := flag.String("origin-listen", "", "optional local HTTPS origin listen address")
	upstreamRaw := flag.String("upstream", "", "upstream HTTP/mixed proxy URL, or ss:// link when --core-bin is set")
	mihomoProxyRaw := flag.String("mihomo-proxy", "", "deprecated alias for --upstream")
	coreKind := flag.String("core", "auto", "core to launch for ss:// upstream: auto, mihomo, or sing-box")
	coreBin := flag.String("core-bin", "", "optional mihomo/sing-box executable used when --upstream is ss://")
	corePort := flag.Int("core-port", 0, "local mixed proxy port for launched core; 0 picks a free port")
	coreHome := flag.String("core-home", "", "optional working directory for generated core config/logs")
	configPath := flag.String("config", "", "AnixOps-style config fixture")
	bilibiliDemo := flag.Bool("bilibili-demo", false, "use the built-in www.bilibili.com HTML MITM demo config")
	caCertPath := flag.String("ca-cert", defaultCACert, "path to write/read test CA certificate")
	caKeyPath := flag.String("ca-key", defaultCAKey, "path to write/read test CA key")
	installCA := flag.Bool("install-ca", false, "install the generated CA into the current user's Windows root store")
	debug := flag.Bool("debug", false, "enable verbose request, MITM, upstream, and script logs")
	scriptRunner := flag.String("script-runner", "", "optional Node.js AnixOps script runner path")
	scriptPathURL := flag.String("script-path-url", "", "script URL to map to a local file")
	scriptPathFile := flag.String("script-path-file", "", "local script file for script-path-url")
	scriptStore := flag.String("script-store", "", "optional JSON file backing $persistentStore for the script runner")
	scriptTimeoutMs := flag.String("script-timeout-ms", "5000", "script runner $done timeout in milliseconds")
	flag.Parse()

	if *mihomoProxyRaw != "" && *upstreamRaw == "" {
		*upstreamRaw = *mihomoProxyRaw
	}
	if *upstreamRaw == "" {
		*upstreamRaw = defaultExternalUpstream
	}
	if *configPath == "" && !*bilibiliDemo {
		log.Fatal("--config is required unless --bilibili-demo is set")
	}

	configForEngine := *configPath
	if *bilibiliDemo {
		path, err := writeTempConfig(defaultBilibiliDemoConfig())
		if err != nil {
			log.Fatal(err)
		}
		defer os.Remove(path)
		configForEngine = path
		log.Printf("demo=bilibili config=%s script=%s", path, builtinBilibiliScriptPath)
	}

	eng, err := newEngine(configForEngine)
	if err != nil {
		log.Fatal(err)
	}
	defer eng.close()

	upstreamProxy, coreProcess, coreCleanup, err := resolveUpstream(*upstreamRaw, *coreKind, *coreBin, *corePort, *coreHome, *debug)
	if err != nil {
		log.Fatal(err)
	}
	if coreCleanup != nil {
		defer coreCleanup()
	}
	if coreProcess != nil {
		installSignalCleanup(coreProcess)
	}

	ca, err := loadOrCreateCA(*caCertPath, *caKeyPath)
	if err != nil {
		log.Fatal(err)
	}
	if *installCA {
		if err := installUserCA(*caCertPath); err != nil {
			log.Fatal(err)
		}
	}
	if err := ensureParentDir(*caCertPath); err != nil {
		log.Fatal(err)
	}
	if err := os.WriteFile(*caCertPath, ca.certPEM, 0644); err != nil {
		log.Fatal(err)
	}

	cache := &certCache{ca: ca, certs: make(map[string]tls.Certificate)}
	if *originListen != "" {
		if err := startOrigin(*originListen, cache); err != nil {
			log.Fatal(err)
		}
	}

	ps := &proxyServer{
		engine:          eng,
		certs:           cache,
		upstreamProxy:   upstreamProxy,
		scriptRunner:    *scriptRunner,
		scriptPathURL:   *scriptPathURL,
		scriptPathFile:  *scriptPathFile,
		scriptStore:     *scriptStore,
		scriptTimeoutMs: *scriptTimeoutMs,
		tempDir:         os.TempDir(),
		debug:           *debug,
		httpClient: &http.Client{Transport: &http.Transport{
			Proxy: http.ProxyURL(upstreamProxy),
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
	log.Printf("READY listen=%s origin=%s upstream=%s ca=%s debug=%v", *listen, originLabel(*originListen), upstreamProxy.String(), *caCertPath, *debug)
	log.Printf("BROWSER_PROXY http=%s https=%s", *listen, *listen)
	if *bilibiliDemo {
		log.Printf("BILIBILI_TEST open=https://www.bilibili.com/ expected_header=X-AnixOps-Bilibili-Demo: applied")
	}
	if err := server.Serve(ln); !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
}

type shadowsocksLink struct {
	Name     string
	Method   string
	Password string
	Server   string
	Port     int
}

func defaultCAPaths() (string, string) {
	base, err := os.UserConfigDir()
	if err != nil || base == "" {
		base = "."
	}
	dir := filepath.Join(base, "AnixOps", "mitm_anixops")
	return filepath.Join(dir, "anixops-ca.pem"), filepath.Join(dir, "anixops-ca.key")
}

func ensureParentDir(path string) error {
	dir := filepath.Dir(path)
	if dir == "." || dir == "" {
		return nil
	}
	return os.MkdirAll(dir, 0755)
}

func loadOrCreateCA(certPath, keyPath string) (caMaterial, error) {
	if certPath != "" && keyPath != "" {
		certPEM, certErr := os.ReadFile(certPath)
		keyPEM, keyErr := os.ReadFile(keyPath)
		if certErr == nil && keyErr == nil {
			cert, err := tls.X509KeyPair(certPEM, keyPEM)
			if err != nil {
				return caMaterial{}, err
			}
			leaf, err := x509.ParseCertificate(cert.Certificate[0])
			if err != nil {
				return caMaterial{}, err
			}
			key, ok := cert.PrivateKey.(*rsa.PrivateKey)
			if !ok {
				return caMaterial{}, errors.New("stored CA key is not RSA")
			}
			log.Printf("CA reuse cert=%s key=%s subject=%s", certPath, keyPath, leaf.Subject.CommonName)
			return caMaterial{cert: leaf, key: key, certPEM: certPEM}, nil
		}
	}

	ca, err := newCA()
	if err != nil {
		return caMaterial{}, err
	}
	if certPath != "" {
		if err := ensureParentDir(certPath); err != nil {
			return caMaterial{}, err
		}
		if err := os.WriteFile(certPath, ca.certPEM, 0644); err != nil {
			return caMaterial{}, err
		}
	}
	if keyPath != "" {
		if err := ensureParentDir(keyPath); err != nil {
			return caMaterial{}, err
		}
		keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(ca.key)})
		if err := os.WriteFile(keyPath, keyPEM, 0600); err != nil {
			return caMaterial{}, err
		}
	}
	log.Printf("CA created cert=%s key=%s subject=%s", certPath, keyPath, ca.cert.Subject.CommonName)
	return ca, nil
}

func installUserCA(certPath string) error {
	if runtime.GOOS != "windows" {
		return errors.New("--install-ca is only implemented on Windows")
	}
	cmd := exec.Command("certutil.exe", "-user", "-addstore", "Root", certPath)
	output, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("certutil failed: %w: %s", err, strings.TrimSpace(string(output)))
	}
	log.Printf("CA installed into current user's Root store: %s", certPath)
	return nil
}

func writeTempConfig(text string) (string, error) {
	file, err := os.CreateTemp("", "anixops-config-*.conf")
	if err != nil {
		return "", err
	}
	path := file.Name()
	if _, err := file.WriteString(text); err != nil {
		_ = file.Close()
		_ = os.Remove(path)
		return "", err
	}
	if err := file.Close(); err != nil {
		_ = os.Remove(path)
		return "", err
	}
	return path, nil
}

func defaultBilibiliDemoConfig() string {
	return "[Script]\n" +
		"http-response ^https?:\\/\\/www\\.bilibili\\.com\\/(\\?.*)?$ requires-body=1, script-path=" + builtinBilibiliScriptPath + ", tag=AnixOps.Bilibili.Homepage.Demo\n\n" +
		"[MitM]\n" +
		"hostname = www.bilibili.com\n"
}

func resolveUpstream(raw, coreKind, coreBin string, corePort int, coreHome string, debug bool) (*url.URL, *exec.Cmd, func(), error) {
	parsed, err := url.Parse(raw)
	if err != nil {
		return nil, nil, nil, err
	}
	if strings.EqualFold(parsed.Scheme, "ss") {
		if coreBin == "" {
			return nil, nil, nil, errors.New("ss:// upstream requires --core-bin pointing to mihomo or sing-box")
		}
		ss, err := parseShadowsocksURL(raw)
		if err != nil {
			return nil, nil, nil, err
		}
		return startCoreForSS(ss, coreKind, coreBin, corePort, coreHome, debug)
	}
	if parsed.Scheme != "http" && parsed.Scheme != "https" {
		return nil, nil, nil, fmt.Errorf("unsupported upstream scheme %q; use http://, https://, or ss:// with --core-bin", parsed.Scheme)
	}
	if parsed.Host == "" {
		return nil, nil, nil, fmt.Errorf("upstream missing host: %s", raw)
	}
	return parsed, nil, nil, nil
}

func parseShadowsocksURL(raw string) (shadowsocksLink, error) {
	parsed, err := url.Parse(raw)
	if err != nil {
		return shadowsocksLink{}, err
	}
	if parsed.Scheme != "ss" {
		return shadowsocksLink{}, fmt.Errorf("not an ss URL: %s", raw)
	}
	name, _ := url.QueryUnescape(parsed.Fragment)
	host := parsed.Hostname()
	portText := parsed.Port()
	userInfo := ""
	if parsed.User != nil {
		userInfo = parsed.User.String()
	}

	if host == "" || (parsed.User == nil && portText == "") {
		encoded := strings.TrimPrefix(raw, "ss://")
		if hash := strings.Index(encoded, "#"); hash >= 0 {
			encoded = encoded[:hash]
		}
		decoded, err := decodeSSBase64(encoded)
		if err != nil {
			return shadowsocksLink{}, err
		}
		if hash := strings.Index(decoded, "#"); hash >= 0 {
			name, _ = url.QueryUnescape(decoded[hash+1:])
			decoded = decoded[:hash]
		}
		at := strings.LastIndex(decoded, "@")
		if at < 0 {
			return shadowsocksLink{}, errors.New("ss URL missing server")
		}
		userInfo = decoded[:at]
		hostPort := decoded[at+1:]
		host, portText, err = net.SplitHostPort(hostPort)
		if err != nil {
			return shadowsocksLink{}, err
		}
	} else {
		decodedUser, err := decodeSSBase64(userInfo)
		if err == nil {
			userInfo = decodedUser
		}
		if value, err := url.QueryUnescape(userInfo); err == nil {
			userInfo = value
		}
	}

	colon := strings.Index(userInfo, ":")
	if colon < 0 {
		return shadowsocksLink{}, errors.New("ss URL userinfo must contain method:password")
	}
	port, err := strconv.Atoi(portText)
	if err != nil || port <= 0 || port > 65535 {
		return shadowsocksLink{}, fmt.Errorf("invalid ss port %q", portText)
	}
	if name == "" {
		name = "ss-upstream"
	}
	return shadowsocksLink{
		Name:     name,
		Method:   userInfo[:colon],
		Password: userInfo[colon+1:],
		Server:   host,
		Port:     port,
	}, nil
}

func decodeSSBase64(value string) (string, error) {
	value = strings.TrimSpace(value)
	if slash := strings.Index(value, "/?"); slash >= 0 {
		value = value[:slash]
	}
	value = strings.TrimRight(value, "=")
	if decoded, err := base64.RawURLEncoding.DecodeString(value); err == nil {
		return string(decoded), nil
	}
	if decoded, err := base64.RawStdEncoding.DecodeString(value); err == nil {
		return string(decoded), nil
	}
	for len(value)%4 != 0 {
		value += "="
	}
	if decoded, err := base64.URLEncoding.DecodeString(value); err == nil {
		return string(decoded), nil
	}
	decoded, err := base64.StdEncoding.DecodeString(value)
	return string(decoded), err
}

func startCoreForSS(ss shadowsocksLink, coreKind, coreBin string, corePort int, coreHome string, debug bool) (*url.URL, *exec.Cmd, func(), error) {
	kind := strings.ToLower(coreKind)
	if kind == "auto" {
		base := strings.ToLower(filepath.Base(coreBin))
		switch {
		case strings.Contains(base, "sing"):
			kind = "sing-box"
		default:
			kind = "mihomo"
		}
	}
	if corePort == 0 {
		port, err := pickFreePort()
		if err != nil {
			return nil, nil, nil, err
		}
		corePort = port
	}
	if coreHome == "" {
		dir, err := os.MkdirTemp("", "anixops-core-*")
		if err != nil {
			return nil, nil, nil, err
		}
		coreHome = dir
	}
	if err := os.MkdirAll(coreHome, 0755); err != nil {
		return nil, nil, nil, err
	}

	var configPath string
	var args []string
	switch kind {
	case "mihomo":
		configPath = filepath.Join(coreHome, "mihomo.yaml")
		if err := os.WriteFile(configPath, []byte(renderMihomoSSConfig(ss, corePort, debug)), 0600); err != nil {
			return nil, nil, nil, err
		}
		args = []string{"-d", coreHome, "-f", configPath}
	case "sing-box", "singbox":
		configPath = filepath.Join(coreHome, "sing-box.json")
		if err := os.WriteFile(configPath, []byte(renderSingBoxSSConfig(ss, corePort, debug)), 0600); err != nil {
			return nil, nil, nil, err
		}
		args = []string{"run", "-c", configPath}
	default:
		return nil, nil, nil, fmt.Errorf("unsupported core %q", coreKind)
	}

	cmd := exec.Command(coreBin, args...)
	logPath := filepath.Join(coreHome, "core.log")
	logFile, err := os.Create(logPath)
	if err != nil {
		return nil, nil, nil, err
	}
	cmd.Stdout = logFile
	cmd.Stderr = logFile
	if err := cmd.Start(); err != nil {
		_ = logFile.Close()
		return nil, nil, nil, err
	}
	if err := waitTCP("127.0.0.1", corePort, 8*time.Second); err != nil {
		_ = cmd.Process.Kill()
		_ = logFile.Close()
		return nil, nil, nil, fmt.Errorf("core did not open local mixed proxy: %w (log: %s)", err, logPath)
	}
	cleanup := func() {
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
			_, _ = cmd.Process.Wait()
		}
		_ = logFile.Close()
	}
	upstream, _ := url.Parse(fmt.Sprintf("http://127.0.0.1:%d", corePort))
	log.Printf("core started kind=%s pid=%d proxy=%s config=%s log=%s ss=%s:%d cipher=%s", kind, cmd.Process.Pid, upstream, configPath, logPath, ss.Server, ss.Port, ss.Method)
	return upstream, cmd, cleanup, nil
}

func renderMihomoSSConfig(ss shadowsocksLink, port int, debug bool) string {
	logLevel := "info"
	if debug {
		logLevel = "debug"
	}
	return fmt.Sprintf(`mixed-port: %d
bind-address: 127.0.0.1
allow-lan: false
mode: rule
log-level: %s
ipv6: false
proxies:
  - name: anixops-upstream
    type: ss
    server: %s
    port: %d
    cipher: %s
    password: %q
proxy-groups:
  - name: ANIXOPS
    type: select
    proxies:
      - anixops-upstream
rules:
  - MATCH,ANIXOPS
`, port, logLevel, ss.Server, ss.Port, ss.Method, ss.Password)
}

func renderSingBoxSSConfig(ss shadowsocksLink, port int, debug bool) string {
	level := "info"
	if debug {
		level = "debug"
	}
	config := map[string]interface{}{
		"log": map[string]interface{}{
			"level": level,
		},
		"inbounds": []map[string]interface{}{
			{
				"type":        "mixed",
				"tag":         "mixed-in",
				"listen":      "127.0.0.1",
				"listen_port": port,
			},
		},
		"outbounds": []map[string]interface{}{
			{
				"type":        "shadowsocks",
				"tag":         "anixops-upstream",
				"server":      ss.Server,
				"server_port": ss.Port,
				"method":      ss.Method,
				"password":    ss.Password,
			},
		},
		"route": map[string]interface{}{
			"final": "anixops-upstream",
		},
	}
	data, _ := json.MarshalIndent(config, "", "  ")
	return string(data) + "\n"
}

func pickFreePort() (int, error) {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return 0, err
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port, nil
}

func waitTCP(host string, port int, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	addr := net.JoinHostPort(host, strconv.Itoa(port))
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", addr, 250*time.Millisecond)
		if err == nil {
			_ = conn.Close()
			return nil
		}
		time.Sleep(150 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for %s", addr)
}

func installSignalCleanup(cmd *exec.Cmd) {
	signals := make(chan os.Signal, 2)
	signal.Notify(signals, os.Interrupt)
	go func() {
		sig := <-signals
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
		}
		log.Printf("stopped by signal=%s", sig)
		os.Exit(130)
	}()
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

func (ps *proxyServer) nextRequestID() uint64 {
	ps.mutex.Lock()
	defer ps.mutex.Unlock()
	ps.requestSeq++
	return ps.requestSeq
}

func (ps *proxyServer) debugf(format string, args ...interface{}) {
	if ps.debug {
		log.Printf("DEBUG "+format, args...)
	}
}

func (e engine) rewrite(rawURL string) (action C.anixops_rewrite_action_t, status int, value string, err error) {
	return e.rewriteForPhase(rawURL, phaseRequest)
}

func (e engine) rewriteForPhase(rawURL string, phase C.anixops_phase_t) (action C.anixops_rewrite_action_t, status int, value string, err error) {
	curl := C.CString(rawURL)
	defer C.free(unsafe.Pointer(curl))
	var result C.anixops_rewrite_result_t
	rc := C.anixops_rewrite_evaluate_url(e.ptr, curl, phase, &result)
	if rc != C.ANIXOPS_OK {
		return rewriteNone, 0, "", fmt.Errorf("anixops_rewrite_evaluate_url failed: %d", int(rc))
	}
	return result.action, int(result.status_code), cStringFromArray(unsafe.Pointer(&result.value[0])), nil
}

func (e engine) applyBody(rawURL string, phase C.anixops_phase_t, body []byte) ([]byte, bool, string, error) {
	outCap, ok := bodyRewriteBufferCap(len(body))
	if !ok {
		return body, false, "body rewrite skipped: body too large", nil
	}
	curl := C.CString(rawURL)
	defer C.free(unsafe.Pointer(curl))
	cbody := C.CString(string(body))
	defer C.free(unsafe.Pointer(cbody))
	out := (*C.char)(C.malloc(C.size_t(outCap)))
	if out == nil {
		return nil, false, "", errors.New("body rewrite output allocation failed")
	}
	defer C.free(unsafe.Pointer(out))

	var chain C.anixops_body_rewrite_chain_t
	rc := C.anixops_rewrite_apply_body_chain(e.ptr, curl, phase, cbody, out, C.size_t(outCap), &chain)
	if rc != C.ANIXOPS_OK {
		return nil, false, "", fmt.Errorf("anixops_rewrite_apply_body_chain failed: %d", int(rc))
	}
	message := "no body rewrite matched"
	rewriteCount := int(chain.rewrite_count)
	if rewriteCount > 0 {
		last := chain.rewrites[rewriteCount-1]
		message = cStringFromArray(unsafe.Pointer(&last.message[0]))
	}
	if rewriteCount == 0 || chain.rewritten == 0 {
		return body, false, message, nil
	}
	if chain.truncated != 0 || strings.Contains(strings.ToLower(message), "truncated") {
		return body, false, message, nil
	}
	rewritten := []byte(C.GoString(out))
	return rewritten, !bytes.Equal(rewritten, body), message, nil
}

func (e engine) evaluateHeader(rawURL string, phase C.anixops_phase_t, startIndex int, currentValue *string) (C.anixops_header_rewrite_result_t, error) {
	curl := C.CString(rawURL)
	defer C.free(unsafe.Pointer(curl))
	var ccurrent *C.char
	if currentValue != nil {
		ccurrent = C.CString(*currentValue)
		defer C.free(unsafe.Pointer(ccurrent))
	}
	var result C.anixops_header_rewrite_result_t
	rc := C.anixops_rewrite_evaluate_header(e.ptr, curl, phase, C.size_t(startIndex), ccurrent, &result)
	if rc != C.ANIXOPS_OK {
		return result, fmt.Errorf("anixops_rewrite_evaluate_header failed: %d", int(rc))
	}
	return result, nil
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
		timeoutMs:    uint64(result.timeout_ms),
		maxSize:      uint64(result.max_size),
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

func rewriteActionIsBody(action C.anixops_rewrite_action_t) bool {
	switch action {
	case rewriteMockRequestBody,
		rewriteMockResponseBody,
		rewriteRequestBodyReplaceRegex,
		rewriteResponseBodyReplaceRegex,
		rewriteRequestBodyJSONReplace,
		rewriteResponseBodyJSONReplace,
		rewriteRequestBodyJQ,
		rewriteResponseBodyJQ:
		return true
	default:
		return false
	}
}

func rewriteActionIsHeader(action C.anixops_rewrite_action_t) bool {
	switch action {
	case rewriteHeaderReplace,
		rewriteHeaderAdd,
		rewriteHeaderDel,
		rewriteHeaderReplaceRegex,
		rewriteResponseHeaderDel,
		rewriteResponseHeaderReplace,
		rewriteResponseHeaderAdd,
		rewriteResponseHeaderReplaceRegex:
		return true
	default:
		return false
	}
}

func rewriteActionIsHeaderReplaceRegex(action C.anixops_rewrite_action_t) bool {
	return action == rewriteHeaderReplaceRegex || action == rewriteResponseHeaderReplaceRegex
}

func bodyRewriteBufferCap(bodyLen int) (int, bool) {
	if bodyLen+1 > maxBodyRewriteBuffer {
		return 0, false
	}
	capacity := bodyLen + 4096
	if bodyLen <= (maxBodyRewriteBuffer-4096)/4 {
		capacity = bodyLen*4 + 4096
	}
	if capacity < 8192 {
		capacity = 8192
	}
	if capacity > maxBodyRewriteBuffer {
		capacity = maxBodyRewriteBuffer
	}
	return capacity, true
}

func (ps *proxyServer) applyHeaderRewrites(rawURL string, phase C.anixops_phase_t, header http.Header) (http.Header, error) {
	nextHeader := cloneHeader(header)
	startIndex := 0
	for {
		result, err := ps.engine.evaluateHeader(rawURL, phase, startIndex, nil)
		if err != nil {
			return nil, err
		}
		if result.action == rewriteNone {
			return nextHeader, nil
		}
		if int(result.rule_index) < startIndex {
			return nil, fmt.Errorf("header rewrite rule index moved backwards: %d", int(result.rule_index))
		}
		if rewriteActionIsHeaderReplaceRegex(result.action) {
			name := cStringFromArray(unsafe.Pointer(&result.header_name[0]))
			current := nextHeader.Get(name)
			result, err = ps.engine.evaluateHeader(rawURL, phase, int(result.rule_index), &current)
			if err != nil {
				return nil, err
			}
		}
		ps.applyHeaderRewriteResult(nextHeader, rawURL, &result)
		startIndex = int(result.rule_index) + 1
	}
}

func (ps *proxyServer) applyHeaderRewriteResult(header http.Header, rawURL string, result *C.anixops_header_rewrite_result_t) {
	name := cStringFromArray(unsafe.Pointer(&result.header_name[0]))
	value := cStringFromArray(unsafe.Pointer(&result.value[0]))
	message := cStringFromArray(unsafe.Pointer(&result.message[0]))
	if name == "" {
		return
	}
	switch result.action {
	case rewriteHeaderAdd, rewriteResponseHeaderAdd:
		header.Add(name, value)
	case rewriteHeaderReplace, rewriteHeaderReplaceRegex, rewriteResponseHeaderReplace, rewriteResponseHeaderReplaceRegex:
		header.Set(name, value)
	case rewriteHeaderDel, rewriteResponseHeaderDel:
		header.Del(name)
	}
	ps.debugf("header_rewrite url=%s phase=%d action=%d name=%s message=%s", rawURL, int(result.phase), int(result.action), name, message)
}

func (ps *proxyServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodConnect {
		ps.handleConnect(w, r)
		return
	}
	ps.handlePlainHTTP(w, r)
}

func (ps *proxyServer) handlePlainHTTP(w http.ResponseWriter, r *http.Request) {
	id := ps.nextRequestID()
	rawURL := r.URL.String()
	if !r.URL.IsAbs() {
		rawURL = "http://" + r.Host + r.URL.RequestURI()
	}
	ps.debugf("#%d http method=%s url=%s host=%s", id, r.Method, rawURL, r.Host)
	action, status, value, err := ps.engine.rewrite(rawURL)
	if err != nil {
		ps.debugf("#%d rewrite_error=%v", id, err)
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	if writeRewrite(w, action, status, value) {
		ps.debugf("#%d rewrite action=%d status=%d value=%s", id, int(action), status, value)
		return
	}
	rawURL, host, header, body, err := ps.applyRequestScript(rawURL, r.Method, r.Host, r.Header, r.Body)
	if err != nil {
		ps.debugf("#%d request_script_error=%v", id, err)
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	header = ps.prepareUpstreamHeader(rawURL, header)
	resp, err := ps.forward(r.Context().Done(), r.Method, rawURL, host, header, body)
	if err != nil {
		ps.debugf("#%d upstream_error=%v", id, err)
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	defer resp.Body.Close()
	ps.debugf("#%d upstream status=%d content_type=%s encoding=%s", id, resp.StatusCode, resp.Header.Get("Content-Type"), resp.Header.Get("Content-Encoding"))
	if err := ps.applyResponseScript(rawURL, r.Method, header, resp); err != nil {
		ps.debugf("#%d response_script_error=%v", id, err)
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	ps.debugf("#%d done status=%d content_length=%d", id, resp.StatusCode, resp.ContentLength)
	copyResponse(w, resp)
}

func (ps *proxyServer) handleConnect(w http.ResponseWriter, r *http.Request) {
	id := ps.nextRequestID()
	hostOnly := stripPort(r.Host)
	intercept, err := ps.engine.shouldMITM(hostOnly)
	if err != nil {
		ps.debugf("#%d connect host=%s mitm_error=%v", id, r.Host, err)
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	ps.debugf("#%d connect host=%s intercept=%v", id, r.Host, intercept)

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
		ps.debugf("#%d tunnel target=%s", id, r.Host)
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
		ps.debugf("#%d tls_handshake_error host=%s err=%v", id, hostOnly, err)
		return
	}
	defer tlsConn.Close()
	ps.debugf("#%d mitm_tls_established host=%s", id, hostOnly)

	reader := bufio.NewReader(tlsConn)
	for {
		req, err := http.ReadRequest(reader)
		if err != nil {
			ps.debugf("#%d mitm_read_done host=%s err=%v", id, hostOnly, err)
			return
		}
		ps.handleMITMRequest(tlsConn, r.Host, req, id)
	}
}

func (ps *proxyServer) handleMITMRequest(conn net.Conn, connectHost string, req *http.Request, id uint64) {
	defer req.Body.Close()
	host := req.Host
	if host == "" {
		host = connectHost
	}
	rawURL := "https://" + host + req.URL.RequestURI()
	ps.debugf("#%d mitm_request method=%s url=%s", id, req.Method, rawURL)
	action, status, value, err := ps.engine.rewrite(rawURL)
	if err != nil {
		ps.debugf("#%d rewrite_error=%v", id, err)
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	if writeRewriteToConn(conn, action, status, value) {
		ps.debugf("#%d rewrite action=%d status=%d value=%s", id, int(action), status, value)
		return
	}

	rawURL, host, header, body, err := ps.applyRequestScript(rawURL, req.Method, host, req.Header, req.Body)
	if err != nil {
		ps.debugf("#%d request_script_error=%v", id, err)
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	header = ps.prepareUpstreamHeader(rawURL, header)
	resp, err := ps.forward(nil, req.Method, rawURL, host, header, body)
	if err != nil {
		ps.debugf("#%d upstream_error=%v", id, err)
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	defer resp.Body.Close()
	ps.debugf("#%d upstream status=%d content_type=%s encoding=%s", id, resp.StatusCode, resp.Header.Get("Content-Type"), resp.Header.Get("Content-Encoding"))
	if err := ps.applyResponseScript(rawURL, req.Method, header, resp); err != nil {
		ps.debugf("#%d response_script_error=%v", id, err)
		writeRawError(conn, http.StatusBadGateway, err.Error())
		return
	}
	ps.debugf("#%d mitm_response status=%d content_length=%d", id, resp.StatusCode, resp.ContentLength)
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
	action, _, _, err := ps.engine.rewriteForPhase(rawURL, phaseRequest)
	if err != nil {
		return "", "", nil, nil, err
	}
	needsBody := match.requiresBody != 0 || rewriteActionIsBody(action)

	var bodyBytes []byte
	if body != nil {
		bodyBytes, err = io.ReadAll(body)
		if err != nil {
			return "", "", nil, nil, err
		}
	}
	originalHeader := cloneHeader(header)
	originalEncoding := contentEncodingForDecode(originalHeader)
	nextHeader, err := ps.applyHeaderRewrites(rawURL, phaseRequest, originalHeader)
	if err != nil {
		return "", "", nil, nil, err
	}
	bodyIsIdentity := false
	if needsBody || contentEncodingChanged(originalHeader, nextHeader) {
		decodedBody, decoded, decodeErr := decodeBodyForScript(originalEncoding, bodyBytes)
		if decodeErr != nil {
			restoreRawBodyHeader(nextHeader, originalHeader, len(bodyBytes))
			ps.debugf("request_body_decode_fail_open url=%s", rawURL)
			return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
		}
		bodyBytes = decodedBody
		normalizeRequestBodyHeader(nextHeader, len(bodyBytes))
		bodyIsIdentity = true
		if decoded {
			ps.debugf("request_body_decoded url=%s bytes=%d", rawURL, len(bodyBytes))
		}
	}
	rewrittenBody, bodyChanged, bodyMessage, err := ps.engine.applyBody(rawURL, phaseRequest, bodyBytes)
	if err != nil {
		return "", "", nil, nil, err
	}
	if bodyChanged {
		bodyBytes = rewrittenBody
		normalizeRequestBodyHeader(nextHeader, len(bodyBytes))
		bodyIsIdentity = true
		ps.debugf("request_body_rewrite url=%s message=%s bytes=%d", rawURL, bodyMessage, len(bodyBytes))
	}
	if match.kind == scriptNone || ps.scriptRunner == "" {
		return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
	}

	done, err := ps.runScript("request", match, scriptURL, method, nextHeader, 0, nil, bodyBytes)
	if err != nil {
		ps.debugf("request_script_fail_open url=%s tag=%s err=%v", scriptURL, match.tag, err)
		return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
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
		bodyIsIdentity = true
	}
	if !bodyIsIdentity && contentEncodingChanged(originalHeader, nextHeader) {
		decodedBody, _, decodeErr := decodeBodyForScript(originalEncoding, bodyBytes)
		if decodeErr != nil {
			restoreRawBodyHeader(nextHeader, originalHeader, len(bodyBytes))
			ps.debugf("request_body_decode_fail_open url=%s", rawURL)
			return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
		}
		bodyBytes = decodedBody
		bodyIsIdentity = true
	}
	if bodyIsIdentity {
		normalizeRequestBodyHeader(nextHeader, len(bodyBytes))
	}
	return rawURL, host, nextHeader, bytes.NewReader(bodyBytes), nil
}

func (ps *proxyServer) prepareUpstreamHeader(rawURL string, header http.Header) http.Header {
	nextHeader := cloneHeader(header)
	needsPlainText := false
	match, err := ps.engine.script(scriptDispatchURL(rawURL), phaseResponse)
	if err == nil && match.kind != scriptNone && match.requiresBody != 0 {
		needsPlainText = true
	}
	action, _, _, err := ps.engine.rewriteForPhase(rawURL, phaseResponse)
	if err == nil && rewriteActionIsBody(action) {
		needsPlainText = true
	}
	if needsPlainText {
		nextHeader.Set("Accept-Encoding", "identity")
		ps.debugf("identity_encoding url=%s tag=%s script=%s bodyRewrite=%v", rawURL, match.tag, match.scriptPath, rewriteActionIsBody(action))
	}
	return nextHeader
}

func (ps *proxyServer) applyResponseScript(rawURL string, method string, requestHeader http.Header, resp *http.Response) error {
	originalHeader := cloneHeader(resp.Header)
	originalEncoding := contentEncodingForDecode(originalHeader)
	nextHeader, err := ps.applyHeaderRewrites(rawURL, phaseResponse, originalHeader)
	if err != nil {
		return err
	}
	resp.Header = nextHeader
	scriptURL := scriptDispatchURL(rawURL)
	match, err := ps.engine.script(scriptURL, phaseResponse)
	if err != nil {
		return err
	}
	action, _, _, err := ps.engine.rewriteForPhase(rawURL, phaseResponse)
	if err != nil {
		return err
	}
	hasBodyRewrite := rewriteActionIsBody(action)
	encodingChanged := contentEncodingChanged(originalHeader, nextHeader)
	if match.kind == scriptNone && !hasBodyRewrite && !encodingChanged {
		return nil
	}

	rawBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	_ = resp.Body.Close()
	scriptBody, decoded, decodeErr := decodeBodyForScript(originalEncoding, rawBody)
	if decodeErr != nil {
		replaceRawResponseBody(resp, rawBody, originalHeader)
		ps.debugf("response_body_decode_fail_open url=%s", rawURL)
		return nil
	}
	normalizeResponseBodyHeader(resp.Header, len(scriptBody))
	bodyChanged := false
	bodyMessage := ""
	if hasBodyRewrite {
		rewritten, changed, message, err := ps.engine.applyBody(rawURL, phaseResponse, scriptBody)
		if err != nil {
			return err
		}
		bodyMessage = message
		if changed {
			scriptBody = rewritten
			bodyChanged = true
			ps.debugf("response_body_rewrite url=%s message=%s bytes=%d", rawURL, bodyMessage, len(scriptBody))
		}
	}
	if match.kind == scriptNone {
		replaceResponseBody(resp, scriptBody)
		return nil
	}
	done, err := ps.runScript(
		"response",
		match,
		scriptURL,
		method,
		requestHeader,
		resp.StatusCode,
		resp.Header,
		scriptBody)
	if err != nil {
		ps.debugf("response_script_fail_open url=%s tag=%s err=%v", scriptURL, match.tag, err)
		if decoded || encodingChanged {
			replaceRawResponseBody(resp, rawBody, originalHeader)
		} else if bodyChanged {
			replaceResponseBody(resp, scriptBody)
		} else {
			replaceResponseBody(resp, rawBody)
		}
		return nil
	}
	if done.Headers != nil {
		resp.Header = make(http.Header)
		for key, value := range done.Headers {
			resp.Header.Set(key, value)
		}
	}
	if done.Body != nil {
		scriptBody = []byte(*done.Body)
	}
	replaceResponseBody(resp, scriptBody)
	if status, ok := numericStatus(done.Status); ok {
		resp.StatusCode = status
		resp.Status = fmt.Sprintf("%d %s", status, http.StatusText(status))
	}
	return nil
}

var errDecodedBodyTooLarge = errors.New("decoded body exceeds rewrite buffer")

func contentEncodingForDecode(header http.Header) string {
	return strings.Join(header.Values("Content-Encoding"), ",")
}

func contentEncodingChanged(original http.Header, rewritten http.Header) bool {
	return contentEncodingForDecode(original) != contentEncodingForDecode(rewritten)
}

func decodeBodyForScript(encoding string, body []byte) ([]byte, bool, error) {
	normalized := strings.ToLower(strings.TrimSpace(encoding))
	if normalized == "" || normalized == "identity" {
		return body, false, nil
	}
	if strings.Contains(normalized, ",") {
		return nil, false, fmt.Errorf("stacked content-encoding is not supported for script body: %s", encoding)
	}

	var reader io.ReadCloser
	var err error
	switch normalized {
	case "gzip", "x-gzip":
		reader, err = gzip.NewReader(bytes.NewReader(body))
	case "deflate":
		reader, err = zlib.NewReader(bytes.NewReader(body))
		if err != nil {
			reader = flate.NewReader(bytes.NewReader(body))
			err = nil
		}
	default:
		return nil, false, fmt.Errorf("unsupported content-encoding for script body: %s", encoding)
	}
	if err != nil {
		return nil, false, err
	}
	defer reader.Close()
	limited := io.LimitReader(reader, int64(maxBodyRewriteBuffer)+1)
	decoded, err := io.ReadAll(limited)
	if err != nil {
		return nil, false, err
	}
	if len(decoded) > maxBodyRewriteBuffer {
		return nil, false, errDecodedBodyTooLarge
	}
	return decoded, true, nil
}

func restoreRawBodyHeader(header http.Header, originalHeader http.Header, bodyLen int) {
	header.Del("Content-Encoding")
	for _, value := range originalHeader.Values("Content-Encoding") {
		header.Add("Content-Encoding", value)
	}
	header.Del("Transfer-Encoding")
	header.Set("Content-Length", strconv.Itoa(bodyLen))
}

func normalizeRequestBodyHeader(header http.Header, bodyLen int) {
	header.Del("Content-Encoding")
	header.Del("Transfer-Encoding")
	header.Set("Content-Length", strconv.Itoa(bodyLen))
}

func normalizeResponseBodyHeader(header http.Header, bodyLen int) {
	header.Del("Content-Encoding")
	header.Del("Transfer-Encoding")
	header.Set("Content-Length", strconv.Itoa(bodyLen))
}

func replaceResponseBody(resp *http.Response, body []byte) {
	resp.Body = io.NopCloser(bytes.NewReader(body))
	resp.ContentLength = int64(len(body))
	resp.TransferEncoding = nil
	resp.Uncompressed = false
	resp.Trailer = nil
	normalizeResponseBodyHeader(resp.Header, len(body))
}

func replaceRawResponseBody(resp *http.Response, body []byte, originalHeader http.Header) {
	resp.Body = io.NopCloser(bytes.NewReader(body))
	resp.ContentLength = int64(len(body))
	resp.TransferEncoding = nil
	resp.Uncompressed = false
	resp.Trailer = nil
	restoreRawBodyHeader(resp.Header, originalHeader, len(body))
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
	if match.scriptPath == builtinBilibiliScriptPath {
		ps.debugf("builtin_script phase=%s tag=%s url=%s body_bytes=%d", phase, match.tag, scriptURL, len(bodyBytes))
		return runBuiltinBilibiliScript(phase, responseStatus, responseHeader, bodyBytes), nil
	}
	if match.maxSize > 0 && uint64(len(bodyBytes)) > match.maxSize {
		return scriptDone{}, fmt.Errorf("script body exceeds max-size %d", match.maxSize)
	}
	if ps.scriptRunner == "" {
		return scriptDone{}, fmt.Errorf("no script runner configured for %s", match.scriptPath)
	}
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
	timeoutMs := ps.scriptTimeoutMs
	if match.timeoutMs > 0 {
		timeoutMs = strconv.FormatUint(match.timeoutMs, 10)
	}

	args := []string{
		ps.scriptRunner,
		"--script", scriptFile,
		"--url", scriptURL,
		"--argument", match.argument,
		"--body", bodyPath,
		"--phase", phase,
		"--method", method,
		"--timeoutMs", timeoutMs,
		"--requestHeaders", string(requestHeaderJSON),
	}
	if ps.scriptStore != "" {
		args = append(args, "--store", ps.scriptStore)
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
	upstreamAddr := proxyTCPAddress(ps.upstreamProxy)
	ps.debugf("tunnel_connect target=%s upstream=%s", target, upstreamAddr)
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
	ps.debugf("tunnel_closed target=%s", target)
}

func runBuiltinBilibiliScript(phase string, responseStatus int, responseHeader http.Header, bodyBytes []byte) scriptDone {
	if phase != "response" {
		return scriptDone{}
	}
	if contentType := responseHeader.Get("Content-Type"); !strings.Contains(strings.ToLower(contentType), "text/html") {
		return scriptDone{}
	}
	headers := flattenHeaders(responseHeader)
	for key := range headers {
		normalized := strings.ToLower(key)
		if normalized == "content-security-policy" ||
			normalized == "content-security-policy-report-only" ||
			normalized == "content-encoding" ||
			normalized == "content-length" {
			delete(headers, key)
		}
	}
	headers["Content-Type"] = "text/html; charset=utf-8"
	headers["Cache-Control"] = "no-store"
	headers["X-AnixOps-Bilibili-Demo"] = "applied"

	body := string(bodyBytes)
	marker := "anixops-bilibili-homepage-demo"
	injection := `<style id="` + marker + `-style">
.bili-video-card__cover img,
.video-card__cover img,
.feed-card img,
.bili-rank-list-video__cover img,
.floor-single-card img,
img[class*="cover"],
img[class*="Cover"],
img[class*="pic"],
img[class*="Pic"] {
  filter: brightness(0) !important;
  background: #000 !important;
}
.bili-video-card__info--tit,
.video-card__info--tit,
.feed-card .title,
.bili-rank-list-video__title,
.floor-single-card .title {
  position: relative !important;
  overflow: hidden !important;
  color: transparent !important;
  text-shadow: none !important;
}
.bili-video-card__info--tit *,
.video-card__info--tit *,
.feed-card .title *,
.bili-rank-list-video__title *,
.floor-single-card .title * {
  visibility: hidden !important;
}
.bili-video-card__info--tit::after,
.video-card__info--tit::after,
.feed-card .title::after,
.bili-rank-list-video__title::after,
.floor-single-card .title::after {
  content: "test";
  position: absolute !important;
  left: 0;
  top: 0;
  right: 0;
  color: #18191c !important;
  visibility: visible !important;
  white-space: nowrap !important;
  overflow: hidden !important;
  text-overflow: ellipsis !important;
  pointer-events: none !important;
}
</style>
<script id="` + marker + `-script">
(() => {
  let attempts = 0;
  function setPageTitle() {
    if (document.title !== "test") {
      document.title = "test";
    }
    const title = document.querySelector("title");
    if (title && title.textContent !== "test") {
      title.textContent = "test";
    }
    attempts += 1;
    if (attempts < 12) {
      setTimeout(setPageTitle, attempts < 4 ? 250 : 1000);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", setPageTitle, { once: true });
  } else {
    setPageTitle();
  }
})();
</script>`
	if !strings.Contains(body, marker) {
		switch {
		case strings.Contains(body, "</head>"):
			body = strings.Replace(body, "</head>", injection+"</head>", 1)
		case strings.Contains(body, "</body>"):
			body = strings.Replace(body, "</body>", injection+"</body>", 1)
		default:
			body += injection
		}
	}
	status := responseStatus
	if status == 0 {
		status = http.StatusOK
	}
	return scriptDone{Status: float64(status), Headers: headers, Body: &body}
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
	case rewriteRedirect301, rewriteRedirect302, rewriteRedirect303, rewriteRedirect307, rewriteRedirect308:
		if status == 0 {
			status = http.StatusFound
		}
		w.Header().Set("Location", value)
		w.WriteHeader(status)
		return true
	case rewriteReject, rewriteReject200, rewriteRejectImg, rewriteRejectVideo, rewriteRejectDict, rewriteRejectArray:
		if status == 0 {
			status = http.StatusForbidden
		}
		w.WriteHeader(status)
		return true
	case rewriteNone:
		return false
	default:
		return false
	}
}

func writeRewriteToConn(conn net.Conn, action C.anixops_rewrite_action_t, status int, value string) bool {
	switch action {
	case rewriteRedirect301, rewriteRedirect302, rewriteRedirect303, rewriteRedirect307, rewriteRedirect308:
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
	case rewriteReject, rewriteReject200, rewriteRejectImg, rewriteRejectVideo, rewriteRejectDict, rewriteRejectArray:
		if status == 0 {
			status = http.StatusForbidden
		}
		writeRawError(conn, status, http.StatusText(status))
		return true
	case rewriteNone:
		return false
	default:
		return false
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
			body := "<!doctype html><html><head><title>origin title</title></head><body><main><a class=\"bili-video-card__info--tit\" href=\"/video/BV1\">origin title</a><img class=\"bili-video-card__cover\" src=\"/cover.jpg\" alt=\"cover\"></main></body></html>"
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			w.Header().Set("X-Origin-Accept-Encoding", r.Header.Get("Accept-Encoding"))
			if r.URL.Query().Get("chunked") == "1" {
				w.Header().Set("X-Origin-Transfer-Mode", "chunked")
				w.WriteHeader(http.StatusOK)
				if flusher, ok := w.(http.Flusher); ok {
					flusher.Flush()
				}
			}
			io.WriteString(w, body)
			return
		}
		if stripPort(r.Host) == "www.bilibili.com" && strings.HasPrefix(r.URL.Path, "/gentleman/polyfill.js") {
			w.Header().Set("Content-Type", "text/javascript; charset=utf-8")
			io.WriteString(w, "console.log('polyfill');\n")
			return
		}
		if strings.HasPrefix(r.URL.Path, "/contract/request-response") {
			body, _ := io.ReadAll(r.Body)
			w.Header().Set("Content-Type", "application/json")
			var payloadFields map[string]interface{}
			if r.URL.Query().Get("summary") == "1" {
				payloadFields = map[string]interface{}{
					"code":             0,
					"requestEncoding":  r.Header.Get("Content-Encoding"),
					"requestByteCount": len(body),
				}
			} else {
				payloadFields = map[string]interface{}{
					"code":            0,
					"requestScript":   r.Header.Get("X-AnixOps-Request-Script"),
					"requestEncoding": r.Header.Get("Content-Encoding"),
					"body":            string(body),
				}
			}
			payload, _ := json.Marshal(payloadFields)
			payload = append(payload, '\n')
			switch r.URL.Query().Get("compressed") {
			case "gzip":
				var compressed bytes.Buffer
				gzipWriter := gzip.NewWriter(&compressed)
				_, _ = gzipWriter.Write(payload)
				_ = gzipWriter.Close()
				w.Header().Set("Content-Encoding", "gzip")
				w.Header().Set("Content-Length", strconv.Itoa(compressed.Len()))
				w.Header().Set("X-Origin-Content-Encoding", "gzip")
				_, _ = w.Write(compressed.Bytes())
			case "deflate":
				var compressed bytes.Buffer
				deflateWriter := zlib.NewWriter(&compressed)
				_, _ = deflateWriter.Write(payload)
				_ = deflateWriter.Close()
				w.Header().Set("Content-Encoding", "deflate")
				w.Header().Set("Content-Length", strconv.Itoa(compressed.Len()))
				w.Header().Set("X-Origin-Content-Encoding", "deflate")
				_, _ = w.Write(compressed.Bytes())
			default:
				_, _ = w.Write(payload)
			}
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
