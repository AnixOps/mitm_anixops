# Game Rewrite Pattern Catalog

A pattern is one reusable mutation shape. A recipe is a sequence of patterns
plus a scope. The catalog exists so that the second, third, and tenth game cost
a selection, not a design.

Each pattern states: intent, authority class it fits, the primitive shape, the
projection per ecosystem, the failure mode when it is used wrongly, and the
evidence class.

---

## P1: Response Projection (displayed value)

**Intent.** The client renders a value that the server already owns. Change
what the client renders.

**Fits.** `local` and `dual`. Wrong for `server` when the same value is later
echoed back by a reconcile call.

```text
# Surge .sgmodule
[URL Rewrite]
^https?:\/\/api\.example\.test\/v1\/profile response-body-json-replace $.data.gold 900000
```

```text
# Loon .plugin
[Body Rewrite]
^https?:\/\/api\.example\.test\/v1\/profile response-body-json-replace $.data.gold 900000
```

```text
# Quantumult X .snippet
#[rewrite_local]
^https?:\/\/api\.example\.test\/v1\/profile url response-body-replace-regex "gold":\d+ "gold":900000
```

**Failure mode.** A `server`-authoritative value that is projected here will be
corrected on the next reconcile, and the correction itself is the anomaly.

**Evidence.** `runner trace` for the matching phase, then `runner replay` with a
fixed input body.

---

## P2: Entity Health (boss, target, enemy)

**Intent.** Shorten or pin the health of the entity a client-side simulation
tracks.

**Fits.** `local` simulation. A server-side instance health is not in the client
payload at all, and a recipe that assumes it is has mis-identified the target.

```text
[URL Rewrite]
^https?:\/\/api\.example\.test\/v1\/battle\/start response-body-json-replace $.boss.hp 1
```

**Failure mode.** Pinning to `1` is fine for a local battle; pinning to `0` ends
the entity before the client's state machine allocates it, which is the classic
"battle never starts" symptom. Use `1`, not `0`.

**Evidence.** `runner replay` proving the field is replaced and the rest of the
body is byte-identical.

---

## P3: Settlement Request (server-authoritative value)

**Intent.** The server computes the ledger entry from the request. Mutate the
request, not the response.

**Fits.** `server` authority with an unsigned or recomputable request.

```text
# Surge .sgmodule, JQ build
[URL Rewrite]
^https?:\/\/api\.example\.test\/v1\/settle http-request-jq '.result.score = (.result.score * 20 | floor)'
```

```text
# Loon .plugin, JQ build
[Body Rewrite]
^https?:\/\/api\.example\.test\/v1\/settle request-body-jq '.result.score = (.result.score * 20 | floor)'
```

**Build note.** JQ execution requires the optional libjq build
(`make JQ=1`). The default build accepts the line and fails open with the
original body, per
[body-mutation-common](../../compatibility/body-mutation-common.md). A recipe
that depends on JQ must state the build requirement in the document.

**Failure mode.** Server-side plausibility checks. Keep the multiplier low and
keep `duration`, `level`, and `score` mutually consistent.

---

## P4: Reward Grant (bounded)

**Intent.** Increase a granted reward while staying inside a distribution the
server already accepts for that input.

**Fits.** `server` and `dual`, with a clamp guard.

```text
# request side, clamped
[URL Rewrite]
^https?:\/\/api\.example\.test\/v1\/settle http-request-jq '.result.gain = ((.result.gain * 1.25 | floor) | if . > 5000 then 5000 else . end)'
```

**Failure mode.** An unbounded multiplier. A single 400x outlier is a stronger
signal than ten 1.25x steps, because the server's own reward table bounds the
former and not the latter.

---

## P5: Gate Removal (ad wall, energy, cooldown)

**Intent.** The client blocks progress behind a client-side gate that the
server does not re-check.

**Fits.** `local` gates, and `dual` gates where the server trusts a client
flag.

```text
# fixed reply, no body parsing needed
#[rewrite_local]
^https?:\/\/api\.example\.test\/v1\/ad\/status url echo-response {"ok":true,"adFree":true,"remain":0}
```

```javascript
// script shape: drop the gate field before the payload reaches the client
let body = JSON.parse($response.body);
body.data = Object.assign({}, body.data, { needAd: false, energy: 9999 });
$done({ body: JSON.stringify(body) });
```

**Failure mode.** A gate that the server re-validates on the next action. The
symptom is a UI that unblocks, then a failure toast one tap later. That is a
`server` gate; move to P3.

---

## P6: Signature Recompute (`adapter-required`)

**Intent.** The settlement request carries a signature over its own body. A
mutation invalidates the signature, so the signature must be recomputed in the
same pass.

**Fits.** `server` authority, signed requests.

This step is `adapter-required`. The policy core dispatches the script and
returns `timeout`, `max-size`, `requires-body`, and argument metadata; the host
runtime executes the JavaScript and writes the result back, per
[script_runtime_contract](../../script_runtime_contract.md).

```text
[Script]
http-request ^https?:\/\/api\.example\.test\/v1\/settle requires-body=1, max-size=262144, timeout=3000, script-path=https://modules.example.com/recipe/sign.js, tag=recipe.example.sign
```

The script computes the same digest the client would, with the same key
material the client uses, and rewrites the signature header and body together.
The recipe must record the exact algorithm, the key location, and the failure
mode when the algorithm rotates.

**Failure mode.** A half-applied mutation: body rewritten, signature stale. The
symptom is a hard rejection with no state change. Always rewrite both in one
script, never in two rules.

---

## P7: Replay With Pacing (`replay`)

**Intent.** One legitimate transaction is idempotent on the server. Repeat it,
paced, within a window that looks like normal play.

**Fits.** `server` authority with idempotent settlement and a token that lives
long enough to be reused.

Workflow:

1. capture one real request with the recon script;
2. `runner replay` it to prove determinism;
3. drive the paced loop outside this library, with `cooldown` between calls and
   a jitter window;
4. stop on the first non-success status.

**Failure mode.** Token expiry and rate heuristics. A replay recipe without a
cooldown, or one that ignores the first rejection, is a recipe defect.

---

## P8: Binary Body (`adapter-required`)

**Intent.** The payload is protobuf, msgpack, or an opaque byte frame.

**Fits.** Any authority class, with a script.

JSON path and JQ steps do not apply. The dispatch reports `binary_body_mode` so
the host can hand the script raw bytes
([body-mutation-common](../../compatibility/body-mutation-common.md)). The
recipe must label this pattern `adapter-required` and must not claim a JSON
step it does not have.

---

## P9: Projection And Ledger Together (`dual`)

**Intent.** Keep the local projection and the settled ledger numerically
consistent so a reconcile does not expose a mismatch.

**Fits.** `dual` authority.

Shape: one request-side clamp (P3 or P4) plus one response-side projection (P1)
using the same factor and the same rounding rule. The two steps carry the same
`clamp-ratio`. The recipe document states the invariant in one line, for
example: `projection == floor(ledger * factor)`.

**Failure mode.** Different rounding on the two sides. A one-unit drift is
harmless; a one-order-of-magnitude drift is the anomaly.

---

## Pattern To Primitive Map

| Pattern | Primitive | Build or boundary |
| --- | --- | --- |
| P1 response projection | `response-body-json-replace` | policy core |
| P2 entity health | `response-body-json-replace` | policy core |
| P3 settlement request | `http-request-jq`, `request-body-jq` | optional libjq |
| P4 bounded grant | JQ clamp expression | optional libjq |
| P5 gate removal | `echo-response`, response script | policy core plus host runtime |
| P6 signature recompute | `[Script]` `http-request` | host runtime |
| P7 replay | runner `replay` | runner, outside library |
| P8 binary body | `binary-body-mode` dispatch | host runtime |
| P9 dual consistency | P3 plus P1 | optional libjq plus policy core |

## Anti-Patterns

- **Scope widening.** Adding `*.example.com` to make a dead rule match. The rule
  was dead because the host or the path was wrong.
- **Two rules, one path.** Two mutations on the same JSON path in the same
  phase. The result depends on merge order.
- **`0` health.** Ends the entity before the state machine allocates it.
- **Silent JQ.** A JQ step in a default build fails open, so the recipe appears
  to do nothing. State the build requirement.
- **Unpaced replay.** The single strongest anomaly a client can emit.
- **Identity mutation.** Rewriting account ids, device ids, or signature seeds.
  This is outside the family and outside the review checklist.
