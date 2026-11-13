# MITM 插件开发指南

[English](README.en.md)

本文写给想为 AnixOps Net 客户端编写 MITM 插件、重写规则和脚本的第三方开发者。
它说明客户端接受哪些 Loon / Surge / Quantumult X 写法、脚本能用哪些 `$` API、
怎样在本地调试、怎样写配方（recipe），以及怎样提交和贡献。

内容以 2026-11-13 的 `mitm_anixops` 与 `networkcore_anixops` main 分支为准。
逐项的兼容状态以 [`../compatibility/matrix.md`](../compatibility/matrix.md)
和客户端导入插件时给出的支持报告为准；本文与它们不一致时，以它们为准，
并欢迎提 PR 修正本文。

## 目录

1. [先读：使用条款与禁止用途](#1-先读使用条款与禁止用途)
2. [插件是怎样运行的](#2-插件是怎样运行的)
3. [支持的插件格式](#3-支持的插件格式)
4. [远程资源：脚本必须是 https 地址](#4-远程资源脚本必须是-https-地址)
5. [脚本 `$` API](#5-脚本--api)
6. [配方（recipe）格式](#6-配方recipe格式)
7. [夹具、契约和矩阵行](#7-夹具契约和矩阵行)
8. [本地调试](#8-本地调试)
9. [完整示例：ipleak IP 改写](#9-完整示例ipleak-ip-改写)
10. [提交与贡献流程](#10-提交与贡献流程)

## 1. 先读：使用条款与禁止用途

使用和分发本组件、为它编写插件，都受 [`TERMS.md`](../../TERMS.md)
约束。和插件作者直接相关的是“产品补充条款”和第 3 节“禁止用途”，要点如下：

- **只处理你有权处理的流量。**插件只能用于你自己的设备和流量，
  或已取得设备所有者和通信相关方书面授权的测试环境。
- **不收录绕过安全机制的配方和插件。**我们不发布、不推荐、
  也不在 [`../recipes/external-examples.md`](../recipes/external-examples.md)
  中列出针对第三方服务绕过其安全机制的内容，包括篡改游戏结算或数值、
  规避反作弊、伪造支付或订单、绕过身份认证、风控、DRM 或证书校验。
  已列出的一经发现即移除。
- **根证书只能由用户手动生成并信任。**插件不能要求、也无法让客户端静默安装证书，
  `[MITM]` 里也只能写插件确实需要的主机名。
- **不外传数据。**解密后的请求和响应可能含有凭据和个人信息。脚本不得把 Cookie、
  Token、账号、请求体或响应体发送到第三方，日志里也不要打印这些内容。

违反这些条款的插件在审核时会被直接拒绝，见[第 10 节](#10-提交与贡献流程)。

## 2. 插件是怎样运行的

一条插件文本在客户端里经过三层，每层的职责是固定的：

| 层 | 做什么 | 代码位置 |
| --- | --- | --- |
| 导入器 | 读 Loon `.plugin`、Surge `.sgmodule`、Quantumult X `.snippet` / 重写片段；给每一行打支持等级；把各家写法翻译成核心语法；列出需要下载的远程文件 | `networkcore_anixops` 的 `crates/mitm-policy`（`loon_plugin.rs`） |
| 策略核心 | 本仓库的 C 库。决定哪些主机解密、URL 重写、请求头/响应头改写、请求体/响应体改写，并给出脚本调度信息 | 本仓库 `src/`、`include/mitm_anixops.h` |
| 脚本运行时 | 执行 `[Script]` 里的 JavaScript，提供 `$` API | `networkcore_anixops` 的 `crates/script-runtime` |

导入器给每一行一个等级：

- `supported`：完全支持；
- `partial`：支持一个有文档的子集；
- `deferred`：交给宿主另行处理，例如定时脚本；
- `unsupported`：不支持，这一行不会生效，其余行照常工作。

客户端导入插件后会显示这四个计数和逐行说明。**插件作者应以这份报告作为
“我的插件在 AnixOps 上能不能用”的第一手依据。**

一次 HTTP 请求或响应经过的顺序（详见
[`../script_runtime_contract.md`](../script_runtime_contract.md#adapter-ordering)）：

- 请求：MITM 判定 → URL 重写（重定向或拒绝会终止后续步骤）→ 请求头改写 →
  请求体改写 → 请求脚本 → 发往上游；
- 响应：解压和去分块 → 响应头改写 → 响应体改写 → 响应脚本 →
  重新计算长度和编码后返回给应用。

也就是说，**脚本总是在静态规则之后运行**，看到的是静态改写之后的内容。

## 3. 支持的插件格式

### 3.1 文件与元数据

- 接受的文件：Loon `.plugin`、Surge `.sgmodule`、Quantumult X `.snippet`
  和重写片段（包括 `#[rewrite_local]` 这类注释掉的段头，甚至没有段头的片段），
  以及可移植的 `.conf`。
- 单个插件文本最大 8 MiB。
- 头部元数据：`#!name`、`#!desc`、`#!author`、`#!homepage`、`#!icon`、
  `#!category` 和 `#!arguments` 被识别并显示；其他 `#!` 行（例如 Surge
  `#!requirement`）被容忍并忽略。元数据不产生策略行为。配方元数据 `#!recipe*`
  见[第 6 节](#6-配方recipe格式)。

### 3.2 段落

下表是高频子集。完整的接受范围、正反测试和缺口见
[`../compatibility/matrix.md`](../compatibility/matrix.md)。

| 段落 | 接受的写法 | 说明 | 契约 |
| --- | --- | --- | --- |
| `[Argument]` | `Key = input,默认值,tag=...,desc=...`、`switch`、`select` | 用户可调参数；在其他行里用 `{Key}` 或 `argument=[{A},{B}]` 引用 | [loon-argument-section](../compatibility/loon-argument-section.md) |
| `[MITM]` | `hostname = a.example.com, *.example.com, -deny.example.com`，支持 `%APPEND%` | 只解密这里列出的主机；`*.example.com` 不含 `example.com` 本身，需要时两个都写 | [loon-mitm-options](../compatibility/loon-mitm-options.md) |
| `[Rewrite]` / `[URL Rewrite]` | `<url 正则> <目标> 301/302/303/307/308`、`reject`、`reject-200`、`reject-img`、`reject-dict`、`reject-array`、`reject-NNN` 等 | Surge `<url> <替换> <类型>` 和 QX `<url> url <动作>` 会被翻译 | [request-rewrite-common](../compatibility/request-rewrite-common.md) |
| `[Header Rewrite]` | `header-add`、`header-replace`、`header-del`、`header-replace-regex`；Surge 带 `http-request` / `http-response` 前缀的写法 | 头名大小写不敏感，`Set-Cookie` 按独立字段处理 | [header-mutation-common](../compatibility/header-mutation-common.md) |
| `[Body Rewrite]` | Loon `http-response <url 正则> <查找正则> <替换> [<查找> <替换> ...]`、`http-request ...`；`response-body-json-replace $.path 值`；`http-response-jq <url 正则> '<jq 过滤器>'`、`http-request-jq ...` | 正则对按顺序逐对应用；jq 在客户端里用 jaq（Rust 实现）执行 | [body-mutation-common](../compatibility/body-mutation-common.md) |
| `[Script]` | `http-request` / `http-response <url 正则> script-path=https://...,requires-body=true,binary-body-mode=false,max-size=...,timeout=...,tag=...,argument=...,enable=...` | 见第 5 节 | [loon-script-metadata](../compatibility/loon-script-metadata.md) |
| `[Map Local]` | `<url 正则> data="..."` 或 `data=https://...`、`mock-response-body` | 不经源站直接回应；QX `echo-response` 翻译成这里 | [response-rewrite-common](../compatibility/response-rewrite-common.md) |
| `[Rule]` | `URL-REGEX`、`DOMAIN`、`DOMAIN-SUFFIX`、`DOMAIN-KEYWORD` 配 `REJECT` 等策略 | 进入客户端分流模型；插件里的 `FINAL` 不支持（`unsupported`） | [loon-rule-reject](../compatibility/loon-rule-reject.md)、[loon-rule-routing-guard](../compatibility/loon-rule-routing-guard.md) |
| 定时与事件脚本 | `cron "<表达式>" script-path=...`、`network-changed`、`generic`、QX `[task_local]` | 不进入 HTTP 管线，交给宿主调度；各平台是否执行以导入报告为准 | [cron-task-trigger](../compatibility/cron-task-trigger.md) |

正则方言：客户端的核心启用了 PCRE2，常见 PCRE 写法（`(?i)`、`\d`、
非捕获组、命名捕获组、惰性量词等）都可用；替换串支持 `$1`、`${1}`、`\1`、
`${name}`、`$<name>`。本仓库默认构建是 POSIX ERE 加一组 PCRE 兼容写法，
本地调试时请用 `PCRE2=1` 构建，与客户端保持一致（见第 8 节）。

### 3.3 明确不支持或会被拒绝的写法

- `[MITM] skip-server-cert-verify = true`：被拒绝（`unsupported`）。
  客户端不会替插件关闭上游证书校验。
- 头部块正则（header-block regex）等暂无对应实现的写法：标为 `unsupported`，
  不交给核心。
- Surge `$httpAPI`、`$environment`，Quantumult X `$utils`，
  Shadowrocket `$rocket` 等宿主专有 API：脚本运行时没有提供，见第 5 节。

## 4. 远程资源：脚本必须是 https 地址

`script-path=` 和 `[Map Local]` 的 `data=` 远程地址由客户端下载，规则如下：

- **只接受 `https://`。**`http://` 地址会被拒绝；https 地址重定向到 http 也会被拒绝。
- 下载不带 Cookie、不走缓存；单个脚本最大 4 MiB，单个插件最大 8 MiB，
  同时启用的插件和脚本总量各自不超过 8 MiB；`[Map Local]` 远程数据在启动时读取，
  总量不超过 2 MiB。
- **脚本第一次运行前需要用户逐个允许。**导入报告会列出插件引用的全部远程文件。
- 请用固定版本的地址发布（带 commit SHA 或 release tag 的 URL），
  不要指向会变化的分支。地址内容变了，用户看到的就不再是他们审阅过的脚本。

`script-path` 只能是远程 https 地址；本地调试时用 runner 的 `--script-map`
把远程地址映射到本地文件，见第 8 节。

## 5. 脚本 `$` API

### 5.1 运行环境

- 引擎：QuickJS-ng（桌面和 iOS 都用它；JavaScriptCore 绑定作为 Apple 平台备选，
  通过同一套一致性测试）。
- 脚本作为非严格模式的全局代码执行，支持 Promise、`async`/`await` 和定时器。
- 脚本没有文件系统、进程和网络能力，唯一的网络出口是 `$httpClient`。
- 每次调度使用新的运行时实例，脚本之间只能通过 `$persistentStore` 共享状态。

### 5.2 全局对象

下表以 `networkcore_anixops` 的
[`crates/script-runtime`](https://github.com/AnixOps/networkcore_anixops/tree/main/crates/script-runtime)
为准。

| 全局 | 行为 |
| --- | --- |
| `$request` | `url`、`method`、`headers`（对象）；`requires-body=true` 时有 UTF-8 字符串 `body` |
| `$response` | `status`、`statusCode`（同值，给 QX 脚本用）、`headers`；`requires-body=true` 时有 `body` |
| `$argument` | 这一行 `argument=` 的值，**是字符串**。`argument=[{A},{B}]` 会展开成 `A=值&B=值` |
| `$done(value)` | 结束脚本，**第一次调用生效**，之后的调用被忽略。字段见 5.3 |
| `$persistentStore.read(key)` / `.write(value, key)` / `.remove(key)` | 持久化键值存储。key 不超过 256 B，value 不超过 512 KiB，总量不超过 8 MiB。不传 key 时使用 `networkcore.script.<tag>` |
| `$prefs.valueForKey` / `setValueForKey` / `removeValueForKey` | QX 写法，与 `$persistentStore` 是同一个存储；`removeAllValues` 返回 `false` |
| `$notification.post(title, subtitle, content, attach)` | 发通知；`attach` 可以是 URL 字符串或 `{openUrl}` / `{"open-url"}` / `{url}`。每次调度最多 16 条 |
| `$notify(title, subtitle, content, options)` | QX 写法，等同 `$notification.post` |
| `$httpClient.get/post/put/delete/head/options/patch(options, callback)` | 回调签名 `callback(error, response, body)`，`response` 含 `status`、`statusCode`、`headers`。见 5.4 |
| `$task.fetch(options)` | QX 写法，返回 Promise，解析为 `{statusCode, status, headers, body}`，失败时拒绝为 `{error}`。**只在脚本使用 `$task` 且完全没有出现 `$loon` 时定义**，这样 Env.js 这类跨客户端脚本会走 Loon 分支 |
| `$loon` | 字符串 `NetworkCore/<版本>`，可用于环境检测 |
| `$script` | `name`、`type`（`http-request` / `http-response`）、`startTime` |
| `$event` | 事件脚本里为 `{name}` |
| `console.log/info/warn/error/debug` | 输出被收集，每次调度最多 128 行、每行 2 KiB |
| `setTimeout` / `clearTimeout` | 在本次调度的时间预算内运行，最多 64 个 |

环境检测时，Env.js 等常见辅助库会把 AnixOps 识别为 **Loon**。
`$environment`、`$httpAPI`、`$utils`、`$rocket` 都未定义，
依赖它们的分支请先判断 `typeof`。

### 5.3 `$done` 的字段

请求脚本（`http-request`）：

```javascript
// 改写请求后继续发往源站
$done({ url: "https://example.com/new", headers: { "X-Feature": "on" }, body: "{}" });

// 不经源站，直接回应
$done({ response: { status: 200, headers: { "Content-Type": "application/json" }, body: "{}" } });
```

响应脚本（`http-response`）：

```javascript
$done({ status: 200, headers: { "Content-Type": "application/json" }, body: "{\"ok\":true}" });
```

- 字段都可省略，省略即保持不变；`$done()` 或 `$done({})` 表示原样放行。
- `body` 为空字符串表示把正文替换为空。
- `status` 可以是数字，也可以是状态行。
- 改了正文后，客户端会去掉过期的 `Content-Encoding`，并重新计算 `Content-Length`。
- 默认只接受字符串正文。`binary-body-mode=true` 时，`$request.body` /
  `$response.body` 是 `Uint8Array`，另有 QX 风格的 `bodyBytes`（ArrayBuffer）；
  `$done` 的 `body` 可以是 `Uint8Array`、`ArrayBuffer` 或字符串（按 UTF-8 编码）。

### 5.4 `$httpClient`

- 请求经客户端隧道发出，与设备上其他连接走同样的路由。
- 使用 HTTP/1.1 和 `Connection: close`；**不跟随重定向**，3xx 原样交给脚本。
- `options` 可以是 URL 字符串，或 `{url, headers, body, timeout}`；
  `body` 不是字符串时会先 `JSON.stringify`。
- 每次调度最多 16 个请求；错误信息是固定文本，不包含 URL 和内容。
- 没有接入网络出口的宿主会让回调收到错误，脚本应处理 `error`。

### 5.5 预算与失败处理

| 项 | 限制 |
| --- | --- |
| 改写和脚本能看到的正文 | 不超过 1 MiB；更大的正文不经脚本，原样放行 |
| 单行 `max-size` | 正文超过时不运行这个脚本 |
| 单行 `timeout` | 按 Loon / Surge 习惯以秒为单位；iOS 上限 3 秒，桌面默认 5 秒、硬上限 30 秒 |
| QuickJS 堆 | 每次调度 8 MiB；大脚本按源码大小放宽，上限 32 MiB |
| 并发 | 同时运行的脚本堆总量不超过 32 MiB |

**脚本超时、抛异常、超出预算或 `$done` 形状不对，客户端都会原样放行**
（fail-open），并记录脚本的 `tag`，不会让用户的请求失败。
所以脚本不应该依赖“失败就拦截”的语义。

## 6. 配方（recipe）格式

配方是把已支持的写法组合成一个可复现结果的插件文件，加上一组 `#!recipe*`
元数据。它是数据而不是代码：不需要新的解析分支，也不需要 C ABI 变化。

- 格式：[`../recipes/recipe-spec-v1.md`](../recipes/recipe-spec-v1.md)，
  九个 v1 键 `#!recipe`、`#!recipe.id`、`#!recipe.version`、`#!recipe.scope`、
  `#!recipe.authority`、`#!recipe.strategy`、`#!recipe.guard`、
  `#!recipe.evidence`、`#!recipe.reviewed`。
- 写作步骤、审核清单和模板：[`../recipes/adding-a-recipe.md`](../recipes/adding-a-recipe.md)。
- 解析器怎样处理这些键：契约
  [`../compatibility/recipe-metadata.md`](../compatibility/recipe-metadata.md)，
  矩阵行 “Recipe module metadata”（PR #48）。这些键作为 `Plugin`
  诊断被容忍，不产生任何策略行为；`anixops-mitm-runner scan`
  会在 `recipeWarnings` 里提示缺少的键、非法取值、缺少 guard、
  以及 `#!recipe.scope` 与 `[MITM] hostname` 不一致。这些只是警告，不会拒绝加载。
- 具体服务的配方不放进本仓库，放在作者自己的仓库，由
  [`../recipes/external-examples.md`](../recipes/external-examples.md) 登记。
  第 1 节列出的禁止类别不会被登记。

不写 `#!recipe*` 元数据的普通插件同样可以使用；配方格式只是让插件更容易审阅和复现。

## 7. 夹具、契约和矩阵行

当你发现某种 Loon / Surge / QX 写法没有被支持，想让本仓库支持它时，
按 [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) 的流程，在**同一个 PR** 里提交：

1. **契约**：在 `docs/compatibility/` 新增或更新一份契约，说明来源生态、
   接受的写法、解析输出、正反用例和未实现项。规则见
   [`../compatibility/source-contracts.md`](../compatibility/source-contracts.md)。
2. **夹具**：`tests/fixtures/<生态>.<名称>.<扩展名>`，以及一个格式错误的对照夹具，
   例如 `Loon.RecipeMetadata.plugin` 和 `Loon.RecipeMetadata.Unsupported.plugin`。
   夹具只用 `*.example`、`*.test` 这类保留域名。
3. **测试**：在 `tests/test_config.c`（或对应的 `tests/test_*.c`）
   注册 `config/<名称>_fixture_*` 正向和反向测试。
4. **矩阵行**：在 [`../compatibility/matrix.md`](../compatibility/matrix.md)
   新增或更新一行，写明生态、状态（`supported` / `partial` / `planned` /
   `unsupported`）、契约、夹具、正反测试 ID 和缺口。
   状态只有在 GitHub Actions 证明测试通过后才能写成 `supported`。
5. **CHANGELOG**：在 `CHANGELOG.md` 的 Unreleased 下记一条。

`scripts/compatibility-evidence-check.sh` 会检查契约里引用的夹具文件和测试 ID
是否真实存在，以及每个顶层夹具是否都被文档引用。

可以照着 PR #48 写：它新增了契约 `recipe-metadata.md`、三个夹具
`Loon.RecipeMetadata*.plugin`、测试
`config/loon_recipe_metadata_fixture_records_tolerated_keys` /
`config/loon_recipe_metadata_unsupported_keys_are_not_claimed`，以及矩阵行
“Recipe module metadata”。

导入器层（翻译 Loon / Surge / QX 写法）的问题提到 `networkcore_anixops`：
夹具放在 `crates/mitm-policy/tests/fixtures/loon-plugins/`，契约测试放在
`crates/mitm-policy/tests/*_contracts.rs`；脚本 `$` API 的行为用
`crates/script-runtime/tests/fixtures/conformance/<case>.js` 加 `<case>.json`
写成一致性用例，QuickJS 和 JavaScriptCore 都要跑通。

## 8. 本地调试

本地调试只用于开发你自己的插件。给本仓库提 PR 时，验收以 GitHub Actions 结果为准，
本地构建和测试不能作为验收证据。

### 8.1 用 runner 检查核心语法

```sh
# 与客户端一致：PCRE2 正则 + jq
make PCRE2=1 JQ=1 runner

# 解析：规则计数、被忽略和被拒绝的行
build/anixops-mitm-runner scan my.plugin

# 匹配：这个 URL 在 response 阶段命中哪些规则
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://api.example.test/v1/profile --phase response

# 反向：请求阶段、范围外主机都不应命中
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://api.example.test/v1/profile --phase request
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://other.example.test/v1/profile --phase response

# 主机判定：是否会解密
build/anixops-mitm-runner trace --plugin my.plugin \
  --host api.example.test --mitm-enabled --cert-state trusted
```

注意：runner 读的是**核心语法**，也就是 url 在前、动作在后的写法，例如
`^https://api\.example\.test/ response-body-replace-regex 查找 替换`。
Loon `[Body Rewrite]` 里动作在前的 `http-response <url> <查找> <替换>`、
Surge `[URL Rewrite]` 的 `<url> <替换> <类型>` 和 QX `url ...`
这类写法由客户端导入器翻译后才交给核心。这些写法请以客户端导入报告为准，
或者在 runner 里写成等价的核心语法来调试。

### 8.2 离线重放脚本

```sh
build/anixops-mitm-runner replay --plugin my.plugin --fixture cases.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-map https://example.com/my/response.js=./response.js \
  --script-store build/my-store.json \
  --timeout-ms 3000
```

`cases.tsv` 每行一个用例，字段用 Tab 分隔：

```text
case<TAB>名称<TAB>request|response<TAB>url<TAB>正文或 -
```

- `--script-map 远程地址=本地文件`：把 `script-path` 映射到本地文件，不联网；
  也可以用 `--script-bundle manifest.json` 一次映射多个文件，并校验 sha256。
- `--script-store`：用 JSON 文件模拟 `$persistentStore`，多次调用之间共享。
- 同一输入重放两次应得到完全相同的输出，否则说明脚本依赖了隐藏状态。

这个 Node 参考运行器只提供 `$request`、`$response`、`$argument`、
`$persistentStore`、`$done` 和 `$script`。用到 `$httpClient`、`$notification`、
`console`、定时器或 QX API 的脚本，请到客户端上验证。

### 8.3 在客户端里验证

1. 把插件和脚本放到 https 地址（例如 GitHub 仓库里带 commit SHA 的 raw 链接）。
2. 在客户端里按地址导入插件，查看支持报告：每行的等级和原因。
3. 自己生成并手动信任根证书，打开 HTTPS 解密，启用插件，并允许它引用的脚本。
4. 访问目标页面，确认效果。iOS 客户端的开发者模式（在设置里解锁）
   可以查看诊断信息和事件。
5. 改了脚本以后，换一个新的 commit 地址重新导入，不要依赖缓存刷新。

## 9. 完整示例：ipleak IP 改写

这个示例把 [ipleak.net](https://ipleak.net/) 页面显示的 IPv4 地址改成
`233.333.333.333`（故意用一个不可能的地址，一眼就能看出改写生效），
用来确认 MITM 和改写确实在工作。它只改你自己访问 ipleak.net 时看到的页面，
不影响任何其他人。

### 9.1 先弄清数据来自哪里

ipleak.net 页面上的 IP 来自两处：

- 首页 HTML 本身：IP 详情表格和 `jsonData` 属性；
- `ipv4.ipleak.net/?mode=ajax`、`ipv6.ipleak.net/?mode=ajax` 返回的 HTML 片段。

页面**不**读取 `/json/`，所以只改 `/json/` 的规则对页面没有作用。
WebRTC 一栏走 STUN，不是 HTTP，无法用 MITM 改写。
写插件前先用只记录不修改的脚本（见
[`../recipes/adding-a-recipe.md`](../recipes/adding-a-recipe.md#step-3-recon)）
确认数据来源，比直接猜路径省时间。

### 9.2 静态版本（不需要脚本）

`ipleak-demo.sgmodule`：

```text
#!name = ipleak IP 改写演示
#!desc = 把 ipleak.net 页面显示的 IPv4 改成 233.333.333.333，用来确认 MITM 生效

[Body Rewrite]
# 页面上的 IP 来自首页 HTML（IP 详情表、jsonData）和 ipv4/ipv6.ipleak.net/?mode=ajax 返回的 HTML 片段，不走 /json/。
http-response ^https?://(ipv4\.|ipv6\.)?ipleak\.net/(\?.*)?$ [0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3} 233.333.333.333
# 直接打开 https://ipleak.net/json/ 时改 JSON 的 ip 字段。
http-response-jq ^https?://(ipv4\.|ipv6\.)?ipleak\.net/json/?(\?.*)?$ '.ip = "233.333.333.333"'

[MITM]
hostname = %APPEND% ipleak.net, *.ipleak.net
```

逐行说明：

- 第一条 `[Body Rewrite]` 只匹配首页和 `?mode=ajax` 片段（路径只能是 `/`，
  后面可带查询串），不会碰到 `/static/js/...` 这样的脚本文件；
  它把正文里所有 IPv4 形状的文本替换掉。
- 第二条用 jq 只改 JSON 的 `ip` 字段，其他字段保持不变；
  `/json/8.8.8.8` 这类查询别的地址的请求不匹配，不会被改。
- `[MITM]` 同时写 `ipleak.net` 和 `*.ipleak.net`，因为通配符不包含裸域名本身；
  `%APPEND%` 表示追加到用户已有的主机列表，而不是覆盖它。

这份插件和它的行为由 `networkcore_anixops` 的契约测试
`ipleak_demo_rewrites_the_address_the_page_shows` 固定（PR #224，
`crates/mitm-policy/tests/loon_plugin_p1_rewrite_contracts.rs`，
夹具 `crates/mitm-policy/tests/fixtures/loon-plugins/ipleak-demo.sgmodule`）。
测试断言：

- 导入等级为完全支持，`ipleak.net`、`ipv4.ipleak.net`、`ipv6.ipleak.net` 都会被解密；
- ajax 片段里的 `203.0.113.7` 被换成 `233.333.333.333`；
- 首页的详情表和 `jsonData` 都被换掉，而 `jquery-4.0.0.min.js` 和纬度 `37.751`
  这些不是 IP 的文本保持不变；
- `/json/` 的 `ip` 字段被换掉，其余字段不变；
- `/static/js/index.js` 和 `/json/8.8.8.8` 不被改写。

这就是一个好插件应有的测试形状：正向用例证明它改了该改的，
反向用例证明它没碰不该碰的。

### 9.3 脚本版本（演示 `$` API）

同样的 `/json/` 改写也可以用脚本完成，顺便演示 `$argument`、`$response`、
`$persistentStore`、`console` 和 `$done`：

`ipleak-script.plugin`：

```text
#!name = ipleak 脚本改写演示
#!desc = 用脚本把 ipleak.net/json/ 的 ip 字段改成可配置的地址

[Argument]
FakeIP = input,233.333.333.333,tag=显示的地址,desc=替换后显示的 IP

[Script]
http-response ^https?://(ipv4\.|ipv6\.)?ipleak\.net/json/?(\?.*)?$ script-path=https://raw.githubusercontent.com/<you>/<repo>/<commit-sha>/ipleak-json.js, requires-body=true, max-size=65536, timeout=3, tag=ipleak-json, argument=[{FakeIP}]

[MITM]
hostname = %APPEND% ipleak.net, *.ipleak.net
```

`ipleak-json.js`：

```javascript
// $argument 在 AnixOps 上是 "FakeIP=233.333.333.333" 这样的字符串；
// 其他客户端可能给对象，两种都处理。
function readArguments(raw) {
  if (raw && typeof raw === "object") {
    return raw;
  }
  var out = {};
  String(raw || "").split("&").forEach(function (pair) {
    var i = pair.indexOf("=");
    if (i > 0) {
      out[pair.slice(0, i)] = pair.slice(i + 1);
    }
  });
  return out;
}

var args = readArguments(typeof $argument === "undefined" ? "" : $argument);
var fakeIP = args.FakeIP || "233.333.333.333";
var body = $response.body;

try {
  var data = JSON.parse(body);
  if (typeof data.ip === "string") {
    data.ip = fakeIP;
    body = JSON.stringify(data);
    var hits = Number($persistentStore.read("ipleak.hits") || "0") + 1;
    $persistentStore.write(String(hits), "ipleak.hits");
    // 只记录计数，不记录真实 IP。
    console.log("ipleak-json: rewritten, hits=" + hits);
  }
} catch (error) {
  console.log("ipleak-json: body is not JSON, left unchanged");
}

$done({ body: body });
```

要点：

- `script-path` 是带 commit SHA 的 https 地址，脚本内容固定；
- `requires-body=true` 才能拿到正文，`max-size` 让异常大的正文直接跳过脚本；
- 正文不是 JSON 时，脚本原样交还正文，不抛异常；
- 日志不打印真实 IP，符合脱敏要求。

离线重放（`cases.tsv`，字段间是 Tab）：

```text
case	json	response	https://ipleak.net/json/	{"country_code":"US","ip":"203.0.113.7"}
case	not-json	response	https://ipleak.net/json/	not json
```

```sh
build/anixops-mitm-runner replay --plugin ipleak-script.plugin --fixture cases.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-map https://raw.githubusercontent.com/<you>/<repo>/<commit-sha>/ipleak-json.js=./ipleak-json.js \
  --script-store build/ipleak-store.json
```

期望第一个用例的正文变成 `{"country_code":"US","ip":"233.333.333.333"}`，
第二个用例的正文保持 `not json`。

## 10. 提交与贡献流程

### 10.1 发布你的插件

- 插件和脚本放在**你自己的仓库**里，用固定 commit 或 release tag 的 https 地址分发。
  本仓库只定义格式和流程，不收录针对具体服务的插件。
- 仓库里写清楚：插件做什么、`[MITM]` 里每个主机为什么需要、
  已知缺口、怎样关闭或卸载，以及你自己的许可证。
- 想登记到 [`../recipes/external-examples.md`](../recipes/external-examples.md)：
  满足该页的登记要求，然后提一个只改该页的文档 PR，加一行登记。
  登记只是索引，不代表我们审查或认可其内容。

### 10.2 报告兼容性问题

某个 Loon / Surge / QX 插件在 AnixOps 上不工作时，请提 issue，附上：

- 最小复现插件（主机名换成 `*.example` / `*.test`，删除任何凭据）；
- 客户端导入报告里相关行的等级和原因；
- 期望的行为，以及它在原客户端上的表现。

### 10.3 给本仓库提 PR

1. 先 fork 仓库并建分支，一个 PR 只做一件事。
2. 语法或行为变更按[第 7 节](#7-夹具契约和矩阵行)提交契约、夹具、正反测试、
   矩阵行和 CHANGELOG；需要人工处理的外部事项记到
   [`../manual-intervention.md`](../manual-intervention.md)。
3. 不要新增依赖，除非契约里写明了原因、替代方案、许可证影响和 CI 怎样验证。
4. 推送后由 GitHub Actions 运行全部检查（格式、构建、测试、夹具、矩阵、
   安全声明检查等）。CI 全绿是合并的前提。
5. PR 描述回答 [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md)
   “Compatibility Changes” 一节的问题：影响哪个生态、依据哪个契约、
   哪个解析用例、哪个正向和反向测试、改了哪一行矩阵。

### 10.4 审核会直接拒绝的内容

- [第 1 节](#1-先读使用条款与禁止用途)列出的禁止类别：篡改游戏结算或数值、
  规避反作弊、伪造支付或订单、绕过身份认证、风控、DRM 或证书校验；
- `[MITM]` 范围比插件需要的大，例如 `*` 或大面积通配；
- 脚本把 Cookie、Token、账号、请求体或响应体发往第三方，或在日志里打印它们；
- 混淆或压缩到无法审阅的脚本，或从会变化的分支地址加载的脚本；
- 要求关闭证书校验，或要求客户端自动安装、信任证书。
