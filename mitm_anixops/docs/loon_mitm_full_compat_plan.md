# LOON MITM Plugin Full Compatibility Plan

本文档用于排版前的方案草稿。目标不是把 `mitm_anixops` 变成 GUI 客户端，而是把它扩展成一个可嵌入、无 UI、跨平台的 LOON/AnixOps MITM 插件兼容工具链。

## 目标

1. 补齐兼容性缺口：更完整的 JQ body rewrite、PCRE/NSRegularExpression 行为覆盖、Quantumult X / Surge / LOON 语法覆盖、边界行为一致性。
2. 让常见 LOON MITM 插件能在没有 LOON 客户端的环境中运行：Windows、Linux、macOS、iOS/macOS Network Extension、Rust/Go/C/C++ 宿主都能通过库或无 UI 工具接入。
3. 保持清晰边界：核心库做规则解析、匹配、改写计划和脚本调度；运行时层做 JS/JQ/HTTP body 处理；平台代理层做网络、TLS、证书、HTTP/2、系统权限。

## 已拍板决策

后续实现、拆 issue、排期和兼容声明按以下口径推进：

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

执行口径：

- 第一阶段不做 GUI，不优先做完整平台代理客户端，先把库、runner、corpus、trace、runtime 行为跑稳。
- 对外避免使用未经验证的 “支持所有 LOON 插件” 绝对表述，改用 corpus 覆盖率、兼容矩阵和 E2E trace 证明兼容范围。
- QuickJS、libjq、PCRE2 是桌面增强 runtime 的默认组合；POSIX regex fallback 继续保留给无依赖嵌入场景。
- iOS/macOS Network Extension、JavaScriptCore、NSRegularExpression 对照属于后续 adapter 阶段，不能阻塞桌面 runner 和核心库推进。

## 当前基础

仓库现有的 `mitm_anixops` 已具备这些能力：

- C ABI 策略核心：`include/mitm_anixops.h` 暴露 MITM host 决策、URL rewrite、header rewrite、body rewrite、script dispatch、argument 解析。
- 配置覆盖：已测试 `[Argument]`、`[Script]`、`[MITM]`、`[Rewrite]`，以及 LOON section alias、Quantumult X snippet、Surge `.sgmodule` 子集。
- 改写覆盖：redirect/reject、request/response body regex、JSON path replace、request/response header add/replace/delete/regex replace。
- 正则覆盖：POSIX ERE 加一批 PCRE/ICU 常见写法归一化，包括 `(?i)`/`(?m)`/`(?s)`、`\d`、`\w`、`\s`、`\h`、`\v`、`\xHH`、`\uHHHH`、lazy quantifier、`\A`/`\z`、non-capturing group、named capture、`\Q...\E`。
- 真实插件证据：BiliUniverse Enhanced `.plugin`、`.snippet`、`.sgmodule` fixture 已证明脚本调度、argument 解析和 Node runner 执行链路。

当前明确还没有覆盖：

- 完整 JQ filter 执行。
- 完整 PCRE2/ICU/NSRegularExpression 语义。
- 完整 Quantumult X / Surge / LOON 全量语法和 malformed line 行为。
- 生产级 HTTP/1.1、HTTP/2、压缩、chunk、body streaming。
- 生产级 JavaScript runtime、脚本下载缓存、`$persistentStore` 持久化。
- 动态证书、CA 安装、平台代理捕获、系统权限工作流。

## 推荐架构

### Layer 1: `libmitm_anixops_policy`

定位：稳定 C ABI 策略核心。

职责：

- 解析 LOON/AnixOps、Surge、Quantumult X 配置和插件语法。
- 编译规则、返回诊断、保留 source span 和 unsupported reason。
- 对 request/response 进行 MITM、URL、header、body、script 的匹配决策。
- 返回结构化 mutation plan，不直接操作 socket、TLS、系统证书或 JS runtime。

建议新增模型：

- `anixops_plan_t`：一次 request 或 response 的完整执行计划。
- `anixops_rule_diagnostic_t`：保留平台、section、line、rule kind、支持状态、错误原因。
- `anixops_compat_profile_t`：`LOON_STRICT`、`SURGE_STRICT`、`QX_STRICT`、`PORTABLE`，用于控制边界行为。

### Layer 2: `libmitm_anixops_runtime`

定位：可选运行时库，承接 “插件功能真正执行”。

职责：

- JS runtime：QuickJS 或 JavaScriptCore 后端，暴露 `$request`、`$response`、`$argument`、`$persistentStore`、`$done`。
- JQ runtime：嵌入 libjq 或 jq-compatible engine，支持 `http-request-jq`、`http-response-jq` 和 LOON 对应 JSON rewrite 形态。
- Body pipeline：统一处理 text/json body、charset、gzip/deflate/brotli/zstd 解压和重压、Content-Length/Transfer-Encoding 更新。
- Script asset pipeline：脚本 URL 解析、下载、缓存、版本 pin、digest 校验、离线包加载。
- Runtime policy：timeout、memory limit、max body size、network access policy、script exception 行为。

建议 C ABI 保持可选：

- 核心库不强依赖 JS/JQ，避免把所有平台都绑到同一 runtime。
- runtime 库以 feature/backend 方式编译：`quickjs`、`jsc`、`libjq`、`no-jq`。
- iOS 可优先 JavaScriptCore，桌面/服务端可优先 QuickJS。

### Layer 3: `anixops-mitm-runner`

定位：无 UI 命令行工具和测试宿主。

职责：

- 加载本地或远程 LOON 插件。
- 提供本地 HTTP/HTTPS proxy 模式，便于 Windows/Linux/macOS 验证插件行为。
- 提供 offline replay 模式：输入 HAR/HTTP fixture，输出 mutation 后的 request/response 和 trace。
- 提供 compat scan 模式：扫描插件并输出支持率、未支持规则、风险项。
- 提供 corpus test 模式：批量跑插件语料，生成兼容矩阵。

建议命令：

```sh
anixops-mitm-runner scan plugin.plugin
anixops-mitm-runner replay --plugin plugin.plugin --fixture case.har
anixops-mitm-runner proxy --plugin plugin.plugin --listen 127.0.0.1:19080 --upstream http://127.0.0.1:7890
anixops-mitm-runner trace --plugin plugin.plugin --url https://example.com/api
```

### Layer 4: Platform Adapters

定位：各平台网络和证书能力。

职责：

- Windows：WinHTTP/本地代理、用户证书存储、签名二进制。
- macOS：Network Extension 或本地代理、系统钥匙串、签名/公证。
- Linux：本地代理、iptables/tproxy 可选集成、系统 trust store 分发差异。
- iOS：Network Extension、显式 CA 信任检测、App Review 约束下的脚本策略。
- Rust/Go/C++ 宿主：通过 C ABI 或语言 wrapper 嵌入。

## 兼容性工作流

### 1. 语法语料库

建立 `tests/fixtures/corpus/`：

- `loon/`：真实 LOON 插件、最小化边界 fixture、malformed fixture。
- `surge/`：`.sgmodule`、body rewrite、JQ body rewrite、header rewrite、script、module argument。
- `quantumultx/`：snippet、`rewrite_local`、`rewrite_remote`、`mitm`、script request/response 变体。
- `common/`：跨平台插件，如 BiliUniverse、YouTube、Spotify、TestFlight、App Store、广告过滤类插件。

每个 fixture 附带：

- source URL 或来源说明。
- sha256 digest。
- expected parse result。
- expected match/mutation trace。
- unsupported rule allowlist。

### 2. 兼容矩阵自动生成

把 `docs/compatibility_matrix.md` 从手写清单升级为测试产物输入：

- 每个测试 case 标记 `platform=loon|surge|qx|portable`。
- 每个规则类型标记 `supported|partial|ignored|rejected|out-of-scope`。
- CI 输出 Markdown 和 JSON 两份矩阵。
- release 前禁止出现未解释的 supported regression。

### 3. 行为 trace

新增标准 trace：

```json
{
  "phase": "response",
  "url": "https://api.example.com/v1",
  "matchedRules": [
    {
      "platform": "loon",
      "section": "Script",
      "line": 12,
      "action": "script-response-body",
      "result": "executed"
    }
  ],
  "mutations": [
    {"kind": "header-replace", "name": "Content-Type"},
    {"kind": "body-replace", "bytesBefore": 1024, "bytesAfter": 988}
  ]
}
```

这个 trace 是跨平台一致性的核心证据：同一插件、同一输入，在 C runner、Rust wrapper、Go shim、Windows proxy 上应产生同等 trace。

## JQ 补洞路线

现状只有 JSON path replacement 子集，不能覆盖真正的 jq filter。

分三步：

1. `jq-lite` 过渡层：继续支持当前 JSON path，并加入常见 assignment 形态，如 `.foo = value`、`.items[] |= ...`、`del(.field)`、`with_entries(select(...))` 的有限模式识别。
2. `libjq` 后端：为 `http-request-jq`、`http-response-jq` 接入真正 jq program 编译和执行，支持 pipe、assignment、iterator、select、map、del、walk、test、capture。
3. 资源限制：每次执行设置 max input bytes、max output bytes、timeout、recursion/iteration budget，防止插件卡死 runner。

验收：

- Surge `http-request-jq` / `http-response-jq` fixture 通过。
- LOON/AnixOps 同类 JSON rewrite 规则通过。
- jq compile error、multi-output、empty output、non-object output、invalid JSON 输入行为有明确诊断。
- mutation 后 JSON 序列化和 body framing 在 runner E2E 中一致。

## PCRE / NSRegularExpression 补洞路线

现状是 POSIX ERE 加 PCRE 常见语法归一化。要接近 LOON/NSRegularExpression，建议改为可选 regex backend：

1. 保留 `posix-lite`：纯 C、无依赖、作为最小 portable backend。
2. 增加 `pcre2`：桌面和服务端默认 backend，覆盖 lookahead/lookbehind、backreference、Unicode property、word boundary、atomic/possessive、named backreference 等。
3. 增加 `icu` 或平台 `NSRegularExpression` 对照测试：用于验证 iOS/macOS 行为，必要时在 Darwin adapter 使用系统行为。
4. 扩展 regex feature diagnostics：当前 Alpha 已对 PCRE2-only 特性输出 “requires pcre2 backend”；后续补齐更细的
   “unsupported in portable backend” 和平台 backend 差异说明。

验收：

- URL regex、body regex、header regex、script regex 共用同一个 regex abstraction。
- capture index、named capture、replacement template 在四类场景一致。
- 空匹配、全局替换、CRLF、多行、Unicode word boundary、lookaround、catastrophic backtracking 限制都有测试。

## Quantumult X / Surge / LOON 语法补洞

### LOON / AnixOps

优先补：

- `[Plugin]` metadata、`#!` metadata 容忍规则。
- `[MITM]` 的 `hostname`、`skip-server-cert-verify`、`h2`、`disable-quic`、`ca-p12`、`ca-passphrase` 诊断。
- `[Script]` request/response/header/body 组合、`requires-body` 默认值、`timeout`、`enable`、`argument` 变体。
- `[Rewrite]`、`[URL Rewrite]`、`[Header Rewrite]`、`[Body Rewrite]`、`[Remote *]` alias 的排序和覆盖规则。
- malformed line：区分 “兼容忽略” 与 “硬错误阻止加载”。

### Surge

优先补：

- module patch 语义：`%APPEND%`、`%INSERT%`、metadata、`#!arguments` query-string 参数替换、`#!requirement` 诊断。
- `[URL Rewrite]` 的 `header` mode。
- `[Body Rewrite]` 的连续 regex replacement 和 JQ body rewrite。
- `[Script]` 的完整 attr-list：`type`、`pattern`、`script-path`、`requires-body`、`timeout`、`max-size`、`debug`、`argument`。
- `[MITM]` module 可操作字段：`hostname`、`skip-server-cert-verify`、`tcp-connection`。

### Quantumult X

优先补：

- `#[rewrite_local]`、`#[rewrite_remote]`、`#[mitm]` 与 INI section 双写法。
- `url` prefixed rewrite 的 redirect/reject/script/header/body 变体。
- `hostname =`、`force-http-engine-hosts`、`skip-server-cert-verify` 等 MITM 相关常见字段的解析和诊断。
- script request/response header/body 的 requires-body 推断。
- 因公开一手文档不稳定，QX 以真实语料和社区主流插件作为兼容基准，所有结论必须落到 fixture。

## 运行时行为一致性

统一 pipeline：

1. SNI/host 阶段：MITM host 决策，必要时拒绝 QUIC，使客户端回落 TCP/TLS。
2. TLS 阶段：平台代理生成 leaf cert、完成 ALPN、记录 h2/h1。
3. Request URL 阶段：URL rewrite 或 terminal reject/redirect。
4. Request header 阶段：枚举并应用 header mutation。
5. Request body 阶段：按需 buffering、dechunk、decompress、body regex/JQ、request script。
6. Upstream 阶段：重算 Host、Content-Length、Transfer-Encoding、Content-Encoding。
7. Response header 阶段：header mutation。
8. Response body 阶段：dechunk、decompress、body regex/JQ、response script。
9. Writeback 阶段：recompress 或移除 encoding，重算 framing，输出 trace。

关键边界：

- terminal URL action 后不再继续同阶段非必要规则。
- body 为空时仍允许 `^$` 生成内容。
- 多条 body rewrite 按规则顺序执行，前一条输出作为后一条输入。
- header name 匹配大小写不敏感，但输出保留 adapter 策略。
- multi-value header 明确 append/replace/delete 语义。
- script timeout/exception 默认 fail-open，保留原始 HTTP object，并记录 trace。

## API 规划

短期保持现有 ABI 不破坏，新增 parallel API：

```c
anixops_plan_t *anixops_plan_new(void);
void anixops_plan_free(anixops_plan_t *plan);

int anixops_engine_build_request_plan(
    const anixops_engine_t *engine,
    const anixops_http_request_view_t *request,
    anixops_plan_t *out_plan);

int anixops_engine_build_response_plan(
    const anixops_engine_t *engine,
    const anixops_http_response_view_t *response,
    anixops_plan_t *out_plan);

size_t anixops_plan_operation_count(const anixops_plan_t *plan);
int anixops_plan_copy_operation(
    const anixops_plan_t *plan,
    size_t index,
    anixops_operation_t *out_operation);
```

runtime 层单独提供：

```c
anixops_runtime_t *anixops_runtime_new(const anixops_runtime_options_t *options);
int anixops_runtime_apply_plan(
    anixops_runtime_t *runtime,
    const anixops_plan_t *plan,
    anixops_http_exchange_t *exchange,
    anixops_trace_t *trace);
```

这样老用户继续用现有 `evaluate_*` API，新 runner 和平台代理使用 plan/runtime API。

## 分期

### Phase 0: 方案固化和语料基线

- 建立插件 corpus 目录结构和 fixture metadata 格式。
- 把现有 BiliUniverse、Representative Loon、Representative Surge、Representative QX 纳入 corpus。
- 输出第一版自动兼容矩阵 JSON。
- 明确 unsupported rule 不是失败，而是需要有诊断和 fixture 证明。

### Phase 1: Parser 和诊断补洞

- 增强 LOON/Surge/QX parser。
- 所有 ignored/rejected/partial rule 都返回结构化 diagnostics。
- 已加 `LOON_STRICT`、`SURGE_STRICT`、`QX_STRICT`、`PORTABLE` mode；当前 strict profile 会把 supported section 中
  未被解析的规则行升为 rejected，后续继续补逐平台细节。
- 覆盖 malformed line、section alias、metadata、argument、module patch 行为。

### Phase 2: Regex backend 抽象

- 抽象 regex compile/match/replace。
- 接入 PCRE2 backend。
- 保留 POSIX backend。
- Darwin 上增加 NSRegularExpression/ICU 对照 fixture。
- 给 URL/body/header/script 四类 regex 建统一测试矩阵。

### Phase 3: JQ backend

- 接入 libjq 或等价 engine。
- 支持 Surge JQ Body Rewrite 和 LOON JSON rewrite。
- 定义 multi-output、empty-output、compile-error、runtime-error 行为。
- 加资源限制和 trace。

### Phase 4: JS runtime

- 提供 QuickJS backend。
- Darwin 可选 JavaScriptCore backend。
- 实现 `$request`、`$response`、`$argument`、`$persistentStore`、`$done`。
- 支持 script timeout、exception、async `$done`、body binary/text 策略。
- 把 BiliUniverse E2E 从测试脚本提升为 runtime contract fixture。

### Phase 5: HTTP/TLS runner

- 提供无 UI CLI proxy。
- HTTP/1.1：CONNECT、TLS MITM、request/response parsing、dechunk/decompress/recompress。
- HTTP/2：ALPN、pseudo-header rewrite、stream body buffering、mock/redirect/reject。
- QUIC：对 MITM hostname 执行 reject/drop，让客户端回落。
- 输出可比较 trace。

### Phase 6: Cross-platform packaging

- Linux/macOS/Windows build artifact。
- Alpha tar.gz 内提供 relocatable pkg-config metadata，作为 C/C++ 集成的第一层分发入口。
- Alpha tar.gz 内提供 Go cgo wrapper，后续补版本化 Go module。
- Alpha tar.gz 内提供 CMake config package，后续补跨平台 CMake configure/build CI。
- Alpha tar.gz 内提供 Rust FFI wrapper，后续补版本化 Rust crate 发布。
- iOS/macOS adapter 文档和 sample。
- Release gate：unit、corpus、runner replay、proxy E2E、ABI allowlist、artifact smoke test。

## 验收标准

不能用 “能解析配置” 作为全兼容结论。必须同时满足：

- 真实 LOON 插件 corpus 的 parse 成功率、supported rule 覆盖率、runtime 执行成功率达到版本目标。
- 每个 unsupported rule 都有结构化诊断和兼容模式说明。
- 同一插件 fixture 在 policy-only、runtime replay、proxy E2E 至少两个层级产生一致 trace。
- JQ、regex、script、header/body framing 均有边界测试。
- Windows/Linux/macOS 至少通过 runner replay 和本地 proxy E2E。
- iOS/macOS Network Extension 只能在对应 adapter 验证后声明支持，不能仅凭核心库通过就宣称完整支持。

## 执行 Backlog

### 规则兼容 Backlog

| 优先级 | 模块 | 交付物 | 验收证据 |
| --- | --- | --- | --- |
| P0 | Corpus | Alpha 已提供 `tests/fixtures/corpus/manifest.json`，记录插件来源、sha256、平台、期望规则计数和诊断状态计数；后续扩展 unsupported allowlist 和更大真实插件集 | `anixops-mitm-runner scan --corpus` 生成稳定 JSON 报告并校验实际 sha256 与 accepted/ignored/rejected 诊断计数，`make runner-check` 覆盖当前 manifest |
| P0 | Diagnostics | 每条规则有 parse 状态、line、section、action、backend requirement | malformed fixture 不再只能靠 last-error 判断 |
| P0 | LOON parser | `[Plugin]`、`[Argument]`、`[MITM]`、`[Script]`、`[Rewrite]`、`[URL Rewrite]`、`[Header Rewrite]`、`[Body Rewrite]` section 归一化 | Representative LOON fixture 扩展后通过 unit test |
| P0 | MITM | `hostname`、deny host、wildcard、`%APPEND%`、`skip-server-cert-verify`、`h2`、`disable-quic` 一致决策 | host matrix fixture 覆盖 exact、suffix、wildcard、deny、empty host |
| P0 | Script | Alpha 已覆盖 LOON/Surge/QX request/response script dispatch、`requires-body`、`argument`、`tag`、`enable` dispatch gating、规则级 `timeout`/`max-size` 元数据；后续补更广 malformed 语义 | script dispatch trace 覆盖四类脚本入口 |
| P0 | Rewrite | URL redirect/reject、header add/replace/del、body regex/mock/json path 顺序一致 | request/response pipeline replay trace 稳定 |
| P1 | Surge parser | Alpha 已覆盖 `#!` metadata 诊断、`#!arguments`、`%APPEND%`/`%INSERT%`、attr-list template；后续补更广 corpus 和边界字段 | Surge corpus parse 报告无未知 P1 规则 |
| P1 | Surge body | Alpha 已提供 body rewrite chain API，可按规则顺序串联 `[Body Rewrite]` regex/mock/JSON/JQ body 规则；`http-request-jq`、`http-response-jq` 已解析并可在 `JQ=1` 下执行，后续补更大 Surge corpus trace | jq fixture 输出和 trace 匹配预期 |
| P1 | Quantumult X parser | `#[rewrite_local]`、`#[rewrite_remote]`、`#[mitm]`、INI section、`url` prefixed actions | QX corpus parse 报告无未知 P1 规则 |
| P1 | Header semantics | Alpha 已提供单 header name 的 case-insensitive lookup API，并提供 bounded header list 的 multi-value append/replace/delete、regex replace 和 Set-Cookie 独立字段策略 | header matrix fixture 覆盖 request 和 response |
| P2 | Malformed behavior | Alpha 已覆盖 supported section 中 ignored rule 在 strict profile 下 rejected、portable 下 ignored；后续补 LOON/SURGE/QX 逐规则 malformed 差异 | 同一 malformed corpus 在不同 profile 下输出不同但稳定诊断 |

### Runtime Backlog

| 优先级 | 模块 | 交付物 | 验收证据 |
| --- | --- | --- | --- |
| P0 | Plan API | Alpha 已提供 `anixops_rewrite_build_plan`，聚合 URL/body/header/script 操作；后续补完整 trace schema 和更多 corpus ordering | 现有 `evaluate_*` API 与 plan API 对同一 fixture 输出一致 |
| P0 | Trace | JSON trace schema 和 golden trace fixtures | `runner replay` 输出可 diff 的 trace |
| P0 | Body pipeline | text/json body view、max body size、empty body、binary passthrough | body matrix 覆盖空 body、invalid UTF-8、超限 body |
| P1 | JS runtime | QuickJS backend，支持 `$request`、`$response`、`$argument`、`$persistentStore`、`$done` | BiliUniverse runtime fixture 不依赖测试专用 Node runner |
| P1 | Script cache | Alpha runner 已支持 offline bundle、sha256 digest 校验、digest mismatch/cache miss 诊断；后续补 remote fetch/cache refresh policy | digest mismatch、cache miss、offline mode 均有 fixture |
| P1 | JQ runtime | Alpha libjq backend 已支持 compile/run/error、first output、empty output、invalid JSON、output-buffer fail-open；后续补 timeout/memory limit | jq manual 常用 filter 子集和插件 corpus 均通过 |
| P1 | Compression | gzip/deflate/brotli/zstd decode/re-encode 或 remove encoding 策略 | Content-Encoding 和 Content-Length replay fixture 通过 |
| P2 | Async script | async `$done`、timeout、promise、exception fail-open | timeout/throw/double `$done` fixture 通过 |
| P2 | Persistent store | namespace、read/write、transaction 或 file backend | 多脚本共享状态 fixture 通过 |

### Regex Backlog

| 优先级 | 模块 | 交付物 | 验收证据 |
| --- | --- | --- | --- |
| P0 | Abstraction | `anixops_regex_backend_t`，统一 URL/body/header/script regex | POSIX backend 现有测试全通过 |
| P1 | PCRE2 | Alpha 可选 PCRE2 backend 已覆盖 lookaround/lookbehind、backreference、Unicode property、word boundary、atomic/possessive、named backreference fixture；后续补更大 PCRE2/ICU 差异矩阵 | PCRE2 feature matrix 通过 |
| P1 | Replacement | Alpha 已统一 `$1`、`${1}`、`\1`、`${name}`、`$<name>` replacement 行为 | URL redirect、body regex、header regex 三类 golden test |
| P1 | Empty match | Alpha 已固定 body/header global replace 对 `^`、`$`、`.*?` 空匹配的行为；`\b` 当前在 PCRE2 word-boundary fixture 覆盖 | `^`、`$`、`\b`、`.*?` fixture 通过 |
| P2 | Darwin parity | NSRegularExpression/ICU 对照 runner | macOS/iOS adapter 声明支持前必须跑通 |
| P2 | Safety | match timeout 或 backtracking budget | catastrophic regex fixture 不阻塞 runner |

### Platform Backlog

| 优先级 | 平台 | 交付物 | 验收证据 |
| --- | --- | --- | --- |
| P0 | Portable CLI | `anixops-mitm-runner scan`、`replay`、`trace` | Linux CI 无网络 fixture 通过 |
| P1 | Local proxy | HTTP/1.1 CONNECT + TLS MITM + upstream proxy | Windows/Linux/macOS proxy smoke E2E |
| P1 | Certificates | 本地 CA 生成、leaf cert cache、用户手动信任检测 | cert install/trust/revoked 状态 trace |
| P1 | HTTP/2 | ALPN h2、pseudo-header rewrite、stream body buffering | h2 redirect/mock/body rewrite fixture 通过 |
| P2 | QUIC fallback | MITM hostname 上拒绝 QUIC，让客户端回落 TCP/TLS | QUIC decision trace 和 proxy E2E |
| P2 | iOS/macOS NE | Network Extension adapter sample | 真机或 macOS runner 验证后才声明支持 |
| P2 | Language bindings | Rust crate、Go cgo package、CMake package | wrapper tests 覆盖 config、rewrite、script、trace |

## 覆盖率目标

| 阶段 | Parse 覆盖 | Runtime 覆盖 | 平台覆盖 | 允许声明 |
| --- | --- | --- | --- | --- |
| 0.x 当前 | Representative fixture 和 BiliUniverse 子集 | 测试 runner 子集 | C ABI + Go shim E2E | `Supported subset` |
| 0.5 corpus | 常见 LOON/Surge/QX 插件 80% 规则可解析或有诊断 | replay trace 覆盖 URL/header/body/script 静态规则 | Linux CLI replay | `Corpus-driven compatibility` |
| 0.7 runtime | 常见插件 90% 规则可解析，JQ/PCRE2/JS backend 可选 | BiliUniverse 等真实插件通过 runtime replay | Windows/Linux/macOS runner | `No-UI runner beta` |
| 0.9 proxy | 常见插件 95% 规则可解析，unsupported 均有说明 | JS/JQ/body/compression/script cache 稳定 | 桌面本地 proxy E2E | `Desktop MITM compatibility beta` |
| 1.0 adapter | 真实 LOON 插件 corpus 达到版本定义覆盖率 | replay 与 proxy trace 一致 | 各平台代理分别验收 | 只对已验收平台声明完整支持 |

## Issue 拆分建议

1. `compat-corpus`: 建目录、manifest schema、迁移现有 fixtures、生成 scan 报告。
2. `diagnostics-v1`: 给 parser 增加 per-rule diagnostic 列表，保留 line/section/action/status。
3. `plan-api-v1`: 保留现有 ABI，新增 request/response plan 和 operation 枚举。
4. `regex-backend`: 抽象 regex backend，先迁移 POSIX，再接 PCRE2。
5. `jq-runtime`: 接 libjq backend，只在 runtime/runner 中启用。
6. `js-runtime`: QuickJS backend，替换测试专用 Node runner 的核心能力。
7. `runner-replay`: 输入 HTTP fixture，输出 mutation 后对象和 trace。
8. `runner-proxy`: HTTP/1.1 CONNECT + TLS MITM + body pipeline。
9. `h2-runtime`: HTTP/2 ALPN、pseudo-header、stream rewrite。
10. `platform-packaging`: Windows/Linux/macOS artifact、Rust/Go/CMake binding。

## 风险和取舍

- “所有 LOON MITM 插件” 实际上包含网络、TLS、JS、JQ、存储、平台权限的组合问题，不能只靠 C 规则库完成。
- QX 公开语法资料不稳定，必须语料驱动，逐条 fixture 固化。
- 完整 PCRE2 和 NSRegularExpression 行为不完全等价，跨平台需要 backend 标识和严格模式。
- JQ 嵌入会引入依赖和资源限制问题，建议保持 optional runtime。
- iOS 上脚本执行、远程下载、证书安装都受平台政策约束，需要单独 adapter 和产品策略。

## 结论

推荐把最终产品定义为 “policy core + optional runtime + no-UI runner + platform adapters”。

`mitm_anixops` 继续作为稳定、可嵌入的 C ABI 规则核心；新增 runtime 和 runner 承担真正的 LOON MITM 插件执行。这样既能保持当前库的可移植性，也能逐步把 JQ、PCRE/NSRegularExpression、QX/Surge/LOON 语法和 HTTP/TLS/JS 执行补齐到可验证状态。
