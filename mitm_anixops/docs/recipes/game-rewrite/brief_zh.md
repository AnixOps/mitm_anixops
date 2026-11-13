# 游戏改写配方（中文简版）

对应英文主文档：

- [`../recipe-spec-v1.md`](../recipe-spec-v1.md)：配方模块格式 v1（可扩展性的核心）
- [`README.md`](README.md)：配方族的目标、权威模型、侦察环、信号卫生
- [`patterns.md`](patterns.md)：可复用改写模式目录（P1–P9）
- [`adding-a-game.md`](adding-a-game.md)：新增一款游戏的 SOP 与评审清单
- [`wangzhe-dalong.md`](wangzhe-dalong.md)：`王者来打龙` 完整示例

本页只做中文提要，权威内容以英文主文档为准。

## 一句话结论

配方是**数据**，不是代码：一个文本模块文件 + 一份说明文档，复用策略核已经支持的重写语法；新增一款游戏只加文件，不改 C 源码、不改 ABI、不改兼容矩阵。

## 为什么这么设计

仓库现有文档分三层，配方是第四层，且必须自我隔离：

| 层 | 回答什么 | 位置 |
| --- | --- | --- |
| 兼容契约 | 解析器和策略核接受什么，凭什么证明 | `docs/compatibility/` |
| 架构契约 | TLS、HTTP、分帧、脚本执行、信任归谁 | `docs/architecture/` |
| 配方 | 用已支持的原语，怎么组合出一个完整结果 | `docs/recipes/` |

配方只引用契约，从不宣称能力。配方缺原语时，正确的后续动作是补兼容契约，不是在配方里绕。

## 四步流程

1. **定范围**：列出精确主机名，`#!recipe.scope` 与 `[MITM] hostname` 必须完全一致；通配符必须写理由；范围外主机一律不碰。
2. **判权威**：跑三个探针——离线探针、对账探针、回显探针——结论写进 `#!recipe.authority`（`local` / `server` / `dual`）。
3. **选模式**：从模式目录里挑最小集合。一个模式正常，两个正常（`dual`），三个通常说明该拆成两份配方。
4. **加护栏**：`#!recipe.strategy` 与 `#!recipe.guard` 必填；单次步进受 `clamp-ratio` 约束，同一 key 的两次改写间隔 `cooldown`，并且要有 `Enable` 开关保证可逆。

## 三种权威对应三种打法

| 权威 | 打法 | 原语 |
| --- | --- | --- |
| `local` | 改展示值，最省事 | `response-body-json-replace` |
| `server` | 改服务端真正评估的那一侧（结算请求） | `http-request-jq` / `request-body-jq` |
| `dual` | 请求侧与展示侧用同一系数、同一取整，保证对账无偏差 | 上面两者成对 |

## 模式速查

| 编号 | 场景 | 要点 |
| --- | --- | --- |
| P1 | 展示值 | 只改表现层，`server` 权威下会被对账打回 |
| P2 | 怪物血量 | 钉 `1`，钉 `0` 会让战斗状态机起不来 |
| P3 | 结算请求 | 需要 `make JQ=1`，默认构建 fail-open |
| P4 | 有界发放 | 除法＋上限，禁止无界倍率 |
| P5 | 广告/体力门 | 服务端复核的门禁会「解一次、下一次失败」 |
| P6 | 签名重算 | `adapter-required`：策略核只派发，JS 由宿主执行 |
| P7 | 重放 | 必须配速；无 `cooldown` 的重放是自曝 |
| P8 | 二进制体 | 走 `binary-body-mode` 派发，JSON 步骤失效 |
| P9 | 双边一致 | `projection == floor(ledger * factor)` 必须成立 |

## 扩展性从哪来

- 新增游戏：加 `docs/recipes/game-rewrite/<title>.md` + 一个模块文件，索引里加一行。
- 新增改写形状（现有语法已能表达）：在 `patterns.md` 加一节。
- 新增改写形状（现有语法表达不了）：先补 `docs/compatibility/` 的契约、正负夹具、矩阵行，配方随后才获得这一步。
- `#!recipe.*` 元数据按族版本冻结，新键只在同主版本内追加，且必须可被旧读取器忽略——因此老模块在新构建里行为不变。

## 验证命令

```sh
build/anixops-mitm-runner scan dalong.sgmodule
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://api.dalong.example/v1/settle --phase response
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://api.dalong.example/v1/settle --phase request
build/anixops-mitm-runner replay --plugin dalong.plugin --fixture dalong.tsv
```

同输入两次 `replay` 输出必须逐字节一致；不一致几乎总是同路径两条改写规则，或者脚本带隐藏状态。

## 边界（写配方时必须标注）

- 签名重算、二进制体：`adapter-required`；
- JQ 执行：可选构建，默认 fail-open；
- 压缩解码、TLS、HTTP 分帧：宿主或 shim 负责；
- Stash 投影只覆盖 reject/redirect，body 与 script 步骤没有 Stash 等价写法；
- 配方只观测自己声明的主机。

## 可逆性

每条配方都必须能在不改任何其它配置的前提下关掉：`Enable` 开关置 `false`，或移除模块文件。策略核不落盘任何配方状态，因此关闭是瞬时且彻底的。
