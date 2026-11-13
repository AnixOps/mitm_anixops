# Worked Example: 王者来打龙 (WeChat Mini-Game)

Family: `game-rewrite/v1`. Recipe id: `anixops.recipe.game.dalong`.

This is the reference example for the family. It walks one title end to end:
scope discovery, authority probe, pattern selection, module file, guard,
verification, and known gaps. Every host and path below is a placeholder,
because the values are per-build and per-title; the procedure is the part that
transfers.

Status of this page: `documentation-only`. It introduces no fixture and no
matrix row.

---

## 1. Scope Discovery

A WeChat mini-game is a normal HTTP client. It has an applet package for offline
assets and a small number of API hosts for state. The API hosts are what a
recipe needs.

Three ways to find them, in increasing order of effort:

1. **Client-side debug.** Load the title in a host that exposes a request log.
   Record every host the client contacts between launch and the first reward.
2. **Package inspection.** The applet package contains the host strings the
   client was built with. Extract the strings and rank by request-path density.
3. **Live trace.** Install the pass-through recon script from
   [adding-a-game.md](adding-a-game.md#step-3-recon) and read the recorded
   pairs.

Rank candidates by how many of these they answer:

| Signal | Meaning |
| --- | --- |
| `/login`, `/auth`, `/session` | session established; likely also carries profile |
| `/profile`, `/user`, `/role` | projection surface; P1 lives here |
| `/settle`, `/report`, `/finish`, `/battle/result` | settlement surface; P3 and P4 live here |
| `/ad`, `/reward`, `/energy` | gate surface; P5 lives here |
| `/config`, `/version`, `/notice` | control plane; leave it alone |

Write the result as an exact list. A typical shape:

```text
#!recipe.scope = api.dalong.example
#!recipe.scope = settle.dalong.example

[MITM]
hostname = api.dalong.example, settle.dalong.example
```

Do not add a CDN wildcard. The asset CDN carries no state, and a wildcard there
buys nothing and costs the whole review.

## 2. Authority Probe

| Probe | Observed on this class of title | Conclusion signal |
| --- | --- | --- |
| Offline spend and restart | Wallet usually survives a local restart | `local` or `dual` |
| Reconcile after reconnect | Wallet is frequently corrected on the next shop or claim action | `dual` |
| Echo | Settlement response echoes the request total rather than recomputing it | `server` owns the ledger, client repaints |

For a mini-game with an ad-reward loop the common result is `dual`: a local
wallet for display, a server ledger for grants. Record it, and pick `clamp` with
the P9 pairing.

```text
#!recipe.authority = dual
#!recipe.strategy = clamp
```

## 3. Recon

```text
[Script]
http-response ^https?:\/\/api\.dalong\.example\/v1\/(profile|settle) requires-body=1, max-size=65536, timeout=3000, script-path=https://modules.example.com/recipe/dalong/recon.js, tag=recipe.dalong.recon
```

Expected recon output, one line per response:

```json
{"path":"/v1/profile","status":200,"keys":["gold","diamond","energy","level"],"len":412}
{"path":"/v1/settle","status":200,"keys":["gain","duration","dragonHp","score"],"len":268}
```

Record which keys are projections (rendered) and which are ledger entries
(evaluated by the server). `gold` and `diamond` are projections; `gain` and
`duration` are ledger inputs.

## 4. Pattern Selection

1. **P1** — `response-body-json-replace` on `$.data.gold` for the launch
   projection.
2. **P4** — clamped `http-request-jq` on `$.gain` for the settlement grant.
3. **P9** — same factor and rounding on both sides so a reconcile shows no
   drift.
4. **P2** — optional `$.dragonHp` pin for the local battle simulation, only if
   the battle runs client-side.
5. **P5** — `echo-response` on the ad-status gate, only if the gate is not
   re-validated server-side.

Start with P1 + P4 + P9. Add P2 and P5 only after the first three are verified on
device, because each additional rule increases the mutation surface.

## 5. Module

```text
#!name = 王者来打龙 · 增强模块
#!desc = game-rewrite/v1 reference recipe: projection, clamped settlement, DNS-free scope.
#!recipe = game-rewrite/v1
#!recipe.id = anixops.recipe.game.dalong
#!recipe.version = 0.1.0
#!recipe.scope = api.dalong.example
#!recipe.scope = settle.dalong.example
#!recipe.authority = dual
#!recipe.strategy = clamp
#!recipe.guard = clamp-ratio=0.25, cooldown=90, monotonic=1, host-allowlist=1, max-size=262144
#!recipe.evidence = runner-trace
#!recipe.reviewed = 2026-11-13

[Argument]
Enable = switch,true,tag=[总开关],desc=Turn the recipe off without removing the module.
Reward.Multiplier = input,1.25,tag=[结算倍率],desc=Settlement multiplier, clamped to the guard ratio.
Guard.Clamp = input,0.25,tag=[钳制比例],desc=Maximum relative step per settlement.
Projection.Gold = input,900000,tag=[展示金币],desc=Rendered gold value only.

[MITM]
hostname = api.dalong.example, settle.dalong.example

[URL Rewrite]
^https?:\/\/api\.dalong\.example\/v1\/profile response-body-json-replace $.data.gold 900000
^https?:\/\/api\.dalong\.example\/v1\/settle http-request-jq '.gain = ((.gain * 1.25 | floor) | if . > 5000 then 5000 else . end)'
^https?:\/\/api\.dalong\.example\/v1\/ad\/status url echo-response {"ok":true,"adFree":true,"remain":0}

[Header Rewrite]
^https?:\/\/api\.dalong\.example\/v1\/settle header-add X-Recipe anixops.game.dalong/0.1.0

[Script]
http-response ^https?:\/\/api\.dalong\.example\/v1\/settle requires-body=1, max-size=262144, timeout=3000, script-path=https://modules.example.com/recipe/dalong/settle.js, tag=recipe.dalong.settle, argument=[{Enable},{Reward.Multiplier},{Guard.Clamp}]
```

Notes on the shape:

- The static rule and the JQ rule target different paths, so there is no
  competing body rule on one path.
- `X-Recipe` is a traceability header. A host that logs headers can confirm
  which module produced a mutation. Keep it; it costs nothing and it makes
  differential debugging possible.
- The script is the clamp enforcer. When the JQ build is absent, the JQ rule
  fails open and the script still applies the guard, so the module degrades
  instead of disappearing.

## 6. Clamp Enforcer Script

```javascript
// settle.js — adapter-executed, per docs/script_runtime_contract.md
let args = {};
try {
  args = JSON.parse($argument || "{}");
} catch (e) {
  args = {};
}
if (args.Enable === false || args.Enable === "false") {
  $done({});
} else {
  const ratio = Math.min(Math.max(Number(args.Guard_Clamp || 0.25), 0), 1);
  let body;
  try {
    body = JSON.parse($response.body);
  } catch (e) {
    $done({});
    body = null;
  }
  if (body) {
    const before = Number(body.gain || 0);
    const step = Math.min(before * ratio, 5000 - before > 0 ? 5000 - before : 0);
    body.gain = Math.floor(before + Math.max(step, 0));
    body.duration = Number(body.duration || 0) || 45;
    $done({ body: JSON.stringify(body) });
  }
}
```

The script reads the body only after checking `Enable`, keeps `duration` inside a
plausible window, and never increases a value past the module ceiling. That is
the difference between P4 and an unbounded multiplier.

## 7. Verification

```sh
# 1. parse: expect accepted rules plus one diagnostic per #!recipe.* line
build/anixops-mitm-runner scan dalong.sgmodule

# 2. positive trace: response phase on the settle host matches
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://settle.dalong.example/v1/settle --phase response

# 3. negative trace: the request phase must not match a response-only rule set
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://settle.dalong.example/v1/settle --phase request

# 4. out-of-scope trace: a host outside the scope must produce zero matches
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://api.other-game.example/v1/settle --phase response

# 5. determinism: the same input body must produce the same output body twice
build/anixops-mitm-runner replay --plugin dalong.plugin --fixture dalong.tsv
```

`dalong.tsv` follows the stable runner fixture format:

```text
case<TAB>name<TAB>request|response<TAB>url<TAB>body-or-
1<TAB>settle-clamped<TAB>response<TAB>https://api.dalong.example/v1/settle<TAB>{"gain":1000,"duration":45}
1<TAB>settle-ceiling<TAB>response<TAB>https://api.dalong.example/v1/settle<TAB>{"gain":4999,"duration":45}
2<TAB>profile-projection<TAB>response<TAB>https://api.dalong.example/v1/profile<TAB>{"data":{"gold":12}}
```

Expected: case 1 grows by the clamp step, case 2 stops at the ceiling instead of
overshooting, case 3 replaces only `$.data.gold`.

## 8. Tuning

| Symptom | Cause | Change |
| --- | --- | --- |
| Value repaints after the next tab switch | `dual` with a request-side mutation the server rejects | lower `Reward.Multiplier`, confirm the signature step |
| Reward landrate drops to zero | settlement rejected; request mutated without signature recompute | add P6, or fall back to P1 only |
| Battle never starts | `dragonHp` pinned to `0` | pin to `1` |
| Nothing changes at all | JQ build absent, and the script path is not on the clamp rule | confirm `make JQ=1`, or `runner replay` the script path |
| Module loads, no rules matched | wrong host, or response rule evaluated in request phase | `runner trace` both phases |
| Ad gate unblocks then fails | server-side gate | drop P5 |

## 9. Known Gaps

- **Signature recompute is `adapter-required`.** If the settlement request is
  signed, this recipe needs the P6 script and a host runtime that executes
  JavaScript. The policy core dispatches and returns metadata only.
- **JQ is an optional build.** The default build accepts the line and fails
  open. State the build in the deployment notes.
- **Stash projection is limited.** `http.url-rewrite` covers reject/redirect
  intent only, so this module's body and script steps have no Stash projection.
- **Compression is adapter-owned.** A host that does not decode gzip or deflate
  before body rules will fail open; the Alpha shim decodes within its bounded
  budget.
- **Binary payloads are out of scope for the JSON steps.** If the title ships
  protobuf, the recipe reduces to P8 and the JSON lines are dead.

## 10. Reversal

Set `Enable` to `false`, or remove the module file from the host. No policy-core
state is written, and the `[Argument]` switch disables the script step without
unloading the module.
