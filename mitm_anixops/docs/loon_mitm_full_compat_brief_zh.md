# LOON MITM 插件全平台兼容方案

## 一句话目标

把现有 `mitm_anixops` 扩展为一个无 UI、可嵌入、跨平台的 LOON MITM 插件兼容工具链，让 LOON/AnixOps 插件能力可以在 Windows、Linux、macOS、iOS/macOS Network Extension，以及 Rust/Go/C/C++ 宿主中复用。

当前 v1.0.0 policy-core 依赖决策以
`docs/architecture/script-runtime-dependency.md` 为准：C policy core 不嵌入
QuickJS、JavaScriptCore 或其他生产 JavaScript 引擎。本文中 QuickJS/JSC
内容属于长期 runtime/adapter backlog，不能作为当前支持声明。

## 已拍板方向

| 决策项 | 选择 |
| --- | --- |
| 产品形态 | 无 UI runner + 可嵌入库优先 |
| 平台优先级 | Windows/Linux/macOS 桌面先行，iOS/macOS Network Extension 后置 |
| 兼容声明口径 | 高兼容 LOON MITM 运行时，以真实插件 corpus 验证覆盖率 |
| JavaScript runtime | 桌面默认 QuickJS，Apple 平台可选 JavaScriptCore |
| JQ 实现 | 直接接 libjq 或 jq-compatible backend |
| Regex backend | 保留 POSIX fallback，桌面默认 PCRE2，Darwin 做 NSRegularExpression/ICU 对照 |
| 第一批 corpus | 先做 10-20 个主流 MITM 插件 |
| 失败策略 | 默认 fail-open，保留原请求/响应并记录 trace 和诊断 |

简化成一句执行口径：

> 先做无 UI runner 和可嵌入库；桌面三平台优先；以真实主流插件 corpus 验证兼容率；QuickJS + libjq + PCRE2 作为默认增强 runtime；Apple 平台后续补 JavaScriptCore 和 NSRegularExpression/ICU 对照；iOS/Network Extension 后置；运行时失败默认 fail-open 并记录 trace。

## 背景

现有 `mitm_anixops` 已经具备规则解析和策略决策基础：支持 MITM host 判断、URL rewrite、header rewrite、body rewrite、script dispatch、Argument 解析，并已用 BiliUniverse Enhanced、Representative LOON、Surge、Quantumult X fixtures 做过验证。

当前短板主要不在 “是否能识别几条规则”，而在是否能完整复刻 LOON MITM 插件运行环境：

- JQ rewrite 已有可选 libjq Alpha 基础，但完整资源限制、语料覆盖和边界行为还没补齐。
- 正则能力仍是 POSIX ERE 加部分 PCRE/ICU 语法归一化，不等于完整 PCRE2 或 NSRegularExpression。
- Quantumult X / Surge / LOON 的语法覆盖还不是全量。
- JavaScript runtime、脚本缓存、`$persistentStore`、body 解压重压、HTTP/2、TLS MITM、证书安装等仍由外部 adapter 承担。

因此，最终产品不能只做一个规则解析库。需要拆成 “策略核心 + 可选运行时 + 无 UI runner + 平台适配器” 四层。

## 总体架构

### 1. 策略核心：`libmitm_anixops_policy`

定位：稳定、轻量、可跨语言调用的 C ABI 核心。

职责：

- 解析 LOON/AnixOps、Surge、Quantumult X 插件和配置。
- 编译规则并输出结构化诊断。
- 对请求和响应生成 MITM、rewrite、header、body、script 的匹配结果。
- 返回 mutation plan，不直接处理 TLS、socket、证书、HTTP framing 或 JS 执行。

这层保持小而稳定，适合被桌面客户端、移动端 Network Extension、Rust/Go/C++ 工程直接嵌入。

### 2. 可选运行时：`libmitm_anixops_runtime`

定位：真正执行插件能力的运行时层。

职责：

- 执行 JavaScript 插件，提供 `$request`、`$response`、`$argument`、`$persistentStore`、`$done`。
- 执行 JQ body rewrite，支持 Surge/LOON 常见 JSON 改写语义。
- 管理 body pipeline：charset、gzip/deflate/brotli/zstd 解压重压、Content-Length、Transfer-Encoding。
- 管理脚本资源：Alpha runner 已支持离线包和 digest 校验；后续补下载、缓存刷新、超时和异常处理。

建议 runtime 后端可选：桌面优先 QuickJS，Darwin 平台可选 JavaScriptCore，JQ 使用 libjq 或兼容实现。

### 3. 无 UI 工具：`anixops-mitm-runner`

定位：开发、验证、自动化和桌面使用的命令行工具。

核心模式：

```sh
anixops-mitm-runner scan plugin.plugin
anixops-mitm-runner replay --plugin plugin.plugin --fixture case.har
anixops-mitm-runner trace --plugin plugin.plugin --url https://example.com/api
anixops-mitm-runner proxy --plugin plugin.plugin --listen 127.0.0.1:19080
```

能力：

- 扫描插件并输出支持率、风险、未支持规则。
- 离线重放 HTTP fixture，生成改写后的请求/响应和 trace。
- 启动本地 HTTP/HTTPS MITM proxy，验证真实网络路径。
- 作为跨平台 CI 和兼容矩阵生成工具。

### 4. 平台适配器

定位：处理各平台无法由通用 C 库统一解决的系统能力。

职责：

- Windows：本地代理、用户证书存储、签名二进制。
- macOS：系统钥匙串、Network Extension、本地代理、签名和公证。
- Linux：本地代理、系统 trust store 差异、可选 iptables/tproxy。
- iOS：Network Extension、CA 信任检测、受限脚本策略。
- Rust/Go/C++：通过 C ABI 或 wrapper 接入策略核心和 runtime。

## 重点补洞

### JQ rewrite

目标：从 JSON path replacement 子集升级到真正的 jq filter 执行。

优先支持：

- `.foo = value`
- `.items[] |= ...`
- `del(.field)`
- `select(...)`
- `map(...)`
- `with_entries(...)`
- pipe、assignment、iterator、empty output、multi-output、compile error、runtime error、input-limit fail-open、output-buffer fail-open

验收标准：Surge `http-request-jq` / `http-response-jq`、LOON JSON rewrite、真实插件 corpus 都能生成稳定 trace。

### PCRE / NSRegularExpression

目标：把现有 POSIX regex 兼容层升级为可切换 backend。

方案：

- 保留 `posix-lite`，作为无依赖 fallback。
- 增加 `pcre2`，Alpha 可选 backend 已覆盖 lookaround、backreference、Unicode property、word boundary、
  atomic/possessive、named backreference fixture。
- Darwin 平台增加 NSRegularExpression/ICU 对照测试。
- 当前 Alpha 已对 PCRE2-only 写法输出 “requires pcre2 backend”，后续继续补齐平台 backend 差异诊断。

验收标准：URL regex、body regex、header regex、script regex 共用同一套 backend，capture 和 replacement 行为一致。

### Quantumult X / Surge / LOON 语法

LOON / AnixOps：

- 补齐 `[Plugin]` metadata、`[Argument]`、`[MITM]`、`[Script]`、`[Rewrite]`、`[URL Rewrite]`、`[Header Rewrite]`、`[Body Rewrite]`。
- 明确 `requires-body`、`timeout`、`argument`、`enable`、`tag`、malformed line 行为。

Surge：

- 补齐 `.sgmodule` metadata、`#!arguments`、`%APPEND%`、`%INSERT%`。
- 补齐 `[Body Rewrite]` regex chain 和 JQ body rewrite。
- Alpha 已扩展 `[Script]` attr-list 的 `type`、`pattern`、`script-path`、`requires-body`、`enable`、`timeout`、`max-size`、`argument` 基础元数据；后续补 `debug` 和更广 malformed 语义。

Quantumult X：

- 覆盖 `#[rewrite_local]`、`#[rewrite_remote]`、`#[mitm]` 和 INI section。
- 覆盖 `url` prefixed redirect/reject/script/header/body 变体。
- Alpha 已覆盖 `force-http-engine-hosts` host-list 和 host-list 型 `skip-server-cert-verify`。
- 以真实插件语料为准，逐条 fixture 固化行为。

### 边界行为一致性

需要固定的关键行为：

- terminal URL action 后是否继续执行后续规则。
- 多条 body rewrite 是否按顺序串联。
- 空 body、invalid UTF-8、binary body、超限 body 的处理。
- header 大小写、multi-value、Set-Cookie 的 add/replace/delete 行为。
- script timeout、max-size overflow、exception、double `$done`、missing script asset 的 fail-open 策略。
- gzip/brotli/zstd 解压重压后 Content-Encoding 与 Content-Length 的一致性。
- HTTP/2 pseudo-header rewrite、mock、redirect、body buffering。

## 分阶段路线

### Phase 0：语料和诊断基线

- 建立插件 corpus 和 manifest。
- 纳入现有 BiliUniverse、Representative LOON、Surge、QX fixtures。
- 输出每条规则的支持状态和诊断。
- 自动生成兼容矩阵 JSON/Markdown。

### Phase 1：Parser 和兼容模式

- 增强 LOON/Surge/QX parser。
- 增加 `LOON_STRICT`、`SURGE_STRICT`、`QX_STRICT`、`PORTABLE` mode。
- 明确 ignored/rejected/partial rule 行为。

### Phase 2：Regex backend

- 抽象 regex compile/match/replace。
- 迁移现有 POSIX 实现。
- 接入 PCRE2。
- 增加 Darwin NSRegularExpression/ICU 对照测试。

### Phase 3：JQ runtime

- 接入 libjq 或兼容 engine。
- 支持 request/response JQ body rewrite。
- 增加资源限制、错误诊断和 trace。

### Phase 4：JavaScript runtime

- 接入 QuickJS，Darwin 可选 JavaScriptCore。
- 实现 `$request`、`$response`、`$argument`、`$persistentStore`、`$done`。
- 支持 timeout、exception、async `$done`、script cache。

### Phase 5：HTTP/TLS runner

- 实现无 UI 本地 proxy。
- 支持 HTTP/1.1 CONNECT、TLS MITM、dechunk、decompress、recompress。
- 支持 HTTP/2 ALPN、pseudo-header rewrite、stream body buffering。
- 对 MITM hostname 执行 QUIC fallback 策略。

### Phase 6：跨平台交付

- 输出 Windows/Linux/macOS artifacts。
- Alpha tar.gz 先提供 relocatable pkg-config metadata，方便 C/C++ 宿主直接集成。
- Alpha tar.gz 先提供 Go cgo wrapper，后续补版本化 Go module。
- Alpha tar.gz 先提供 CMake config package，后续补跨平台 CMake configure/build CI。
- Alpha tar.gz 先提供 Rust FFI wrapper，后续补版本化 Rust crate 发布。
- 提供 iOS/macOS Network Extension adapter sample。
- 建立 release gate：unit、corpus、runner replay、proxy E2E、ABI allowlist、artifact smoke test。

## 验收口径

不能用 “能解析配置” 宣称全兼容。完整兼容必须同时满足：

- 真实 LOON 插件 corpus 达到目标覆盖率。
- 每条 unsupported rule 都有结构化诊断。
- 同一插件在 policy-only、runtime replay、proxy E2E 中产生一致 trace。
- JQ、regex、script、header/body framing、HTTP/2、compression 均有边界测试。
- Windows/Linux/macOS 均通过本地 runner 和 proxy E2E。
- iOS/macOS Network Extension 必须由对应 adapter 验证后单独声明支持。

## 覆盖目标

| 阶段 | 可声明状态 |
| --- | --- |
| 当前 | Supported subset |
| 0.5 | Corpus-driven compatibility |
| 0.7 | No-UI runner beta |
| 0.9 | Desktop MITM compatibility beta |
| 1.0 | 按已验收平台声明完整支持 |

## 主要风险

- “所有 LOON MITM 插件” 实际包含规则解析、正则、JQ、JS、HTTP、TLS、证书、存储、平台代理权限多个问题，不能只靠一个 C 库解决。
- QX 公开资料不稳定，必须语料驱动。
- PCRE2 与 NSRegularExpression 行为无法完全等价，需要 backend 标识和严格模式。
- JQ 和 JS runtime 会引入依赖、资源限制和安全边界。
- iOS 上远程脚本、证书信任和 Network Extension 受平台策略影响，必须单独设计。

## 结论

推荐路线是：保留 `mitm_anixops` 作为稳定策略核心，新增 optional runtime 承接 JQ/JS/body pipeline，再提供无 UI runner 作为跨平台验证和使用入口，最后由各平台代理适配网络、TLS、证书和系统权限。

这样可以避免把所有复杂度塞进核心库，同时让 LOON MITM 插件能力逐步变成可测试、可追踪、可跨平台交付的工程体系。
