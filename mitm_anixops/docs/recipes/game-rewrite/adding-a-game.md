# Adding A Game

This is the standard operating procedure for adding one title to the
`game-rewrite/v1` family. It is written so that a new title costs one document
and one module file, and so that two different authors produce compatible
results.

Copy the templates at the end of this page, fill them, run the verification
commands, then open the pull request.

---

## Step 1: Scope

Decide the hosts before anything else. Write them as exact hosts.

```text
api.dalong.example
*.dalong-cdn.example     # only with a stated reason
```

Rules for scope:

- one host per line in both `#!recipe.scope` and `[MITM] hostname`;
- a wildcard needs a justification sentence in the document;
- a host you cannot name belongs to a different recipe;
- the recipe scope and the `[MITM]` list must be identical sets, not subsets.

Why: the policy core only decrypts hosts the adapter was told to decrypt, per
[certificate-lifecycle](../../architecture/certificate-lifecycle.md). A scope
larger than the target means the recipe observes traffic it has no business
observing, and every reviewer reads that as a defect.

## Step 2: Authority

Run the three probes from [README.md](README.md) and write the conclusion.

```text
#!recipe.authority = dual
```

Why: the probe result decides whether the recipe mutates the response, the
request, or both. Skipping the probe is the single most common reason a recipe
"works in the editor and does nothing on device".

## Step 3: Recon

Install a pass-through script on the candidate host and record the traffic
shape. Use the smallest possible budget so recon cannot become a body-buffering
problem.

```text
[Script]
http-response ^https?:\/\/api\.dalong\.example\/v1\/ requires-body=1, max-size=65536, timeout=3000, script-path=https://modules.example.com/recipe/recon.js, tag=recipe.dalong.recon
```

```javascript
// recon.js, per docs/script_runtime_contract.md
const r = $response;
let keys = [];
try {
  keys = Object.keys(JSON.parse(r.body).data || {});
} catch (e) {
  keys = ["<non-json:" + (r.body || "").length + ">"];
}
console.log(JSON.stringify({ status: r.status, keys: keys }));
$done({});
```

Record in the document: host, path, method, phase, top-level keys, and whether
the payload is JSON.

Why: `$done({})` leaves the body untouched, so recon cannot change behavior.

## Step 4: Select Patterns

Choose the smallest set of patterns from [patterns.md](patterns.md) that reaches
the outcome. Then check the count.

- One pattern is normal.
- Two patterns is normal for `dual`.
- Three patterns means the target is probably two recipes, or the authority
  classification is wrong.

Write the selection into the document as a numbered list, each line naming the
pattern id and the primitive.

## Step 5: Parameterize

Every number a user may want to change belongs in `[Argument]`.

```text
[Argument]
Enable = switch,true,tag=[总开关],desc=Turn the recipe off without removing it.
Reward.Multiplier = input,20,tag=[Reward],desc=Settlement multiplier.
Guard.Clamp = input,0.25,tag=[Guard],desc=Maximum relative step.
```

and is referenced from the rule that uses it:

```text
[Script]
http-response ^https?:\/\/api\.dalong\.example\/v1\/settle requires-body=1, max-size=262144, timeout=3000, script-path=https://modules.example.com/recipe/dalong/settle.js, tag=recipe.dalong.settle, argument=[{Enable},{Reward.Multiplier},{Guard.Clamp}]
```

Why: a hard-coded multiplier makes the recipe unauditable and makes the `Enable`
switch impossible, which breaks the reversibility requirement in
[README.md](README.md).

## Step 6: Guard

Apply the strategy guard from
[../recipe-spec-v1.md](../recipe-spec-v1.md#metadata-keys).

```text
#!recipe.strategy = clamp
#!recipe.guard = clamp-ratio=0.25, cooldown=90, monotonic=1, host-allowlist=1, max-size=262144
```

Why: a bounded step plus pacing is the difference between a recipe that keeps
working and one that trips a heuristic during the first burst.

## Step 7: Verify

```sh
# parse, and count the tolerated metadata diagnostics
build/anixops-mitm-runner scan dalong.plugin

# phase and host matching, positive
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://api.dalong.example/v1/settle --phase response

# phase and host matching, negative: a request trace must not match a response rule
build/anixops-mitm-runner trace --plugin dalong.plugin \
  --url https://api.dalong.example/v1/settle --phase request

# determinism
build/anixops-mitm-runner replay --plugin dalong.plugin --fixture dalong.tsv
```

Record the observed counters in the document. A negative trace that matches is a
phase bug in the recipe, not in the library.

## Step 8: Document

One file, `docs/recipes/game-rewrite/<title>.md`, using the template below. Add
one line to the index in
[docs/recipes/README.md](../README.md#recipe-index) and, if the title
introduces a new shape, one row to
[patterns.md](patterns.md).

---

## Review Checklist

A reviewer accepts a new game when every line holds:

- [ ] Scope is exact, justified, and identical in metadata and `[MITM]`.
- [ ] Authority is probed, not assumed, and the probe outcomes are recorded.
- [ ] Pattern selection is minimal and each pattern is named.
- [ ] Every user-tunable number is an `[Argument]` reference.
- [ ] Strategy guard is present and matches the strategy.
- [ ] `runner scan`, positive `trace`, negative `trace`, and `replay` all pass.
- [ ] Every `adapter-required` step is labeled, including JQ builds, script
      execution, binary bodies, and compression.
- [ ] Reversibility is one switch or one module removal.
- [ ] The document states known gaps.
- [ ] No host outside the declared scope appears anywhere in the file.

Rejections in this family are almost always one of: scope widening, unpaced
replay, hard-coded multiplier, or an unlabeled `adapter-required` step.

---

## Promotion To Compatibility Fixtures

A recipe snippet stays documentation until it also proves parser acceptance. If
the snippet introduces a grammar shape that no fixture exercises yet, the next
pull request does all of the following together:

1. add `tests/fixtures/<Ecosystem>.<Name>.<ext>` and a malformed counterpart;
2. register `config/<name>_fixture_*` tests in `tests/test_config.c`;
3. add a paragraph to the matching `docs/compatibility/*.md` contract;
4. update the matrix row;
5. run `sh scripts/check.sh`.

That is the path from a recipe to a capability. Until that pull request lands,
the recipe must use only grammar the existing fixtures already prove, per
[../recipe-spec-v1.md](../recipe-spec-v1.md#primitive-mapping).

---

## Templates

### Module File

```text
#!name = <title> · 增强模块
#!desc = <one line>
#!recipe = game-rewrite/v1
#!recipe.id = anixops.recipe.game.<slug>
#!recipe.version = 0.1.0
#!recipe.scope = <host>
#!recipe.authority = <local|server|dual>
#!recipe.strategy = <observe|clamp|max|replay>
#!recipe.guard = clamp-ratio=0.25, cooldown=90, monotonic=1, host-allowlist=1, max-size=262144
#!recipe.evidence = runner-scan
#!recipe.reviewed = <YYYY-MM-DD>

[Argument]
Enable = switch,true,tag=[总开关]

[MITM]
hostname = <host>

[URL Rewrite]
<pattern line>
```

### Document File

````markdown
# <Title>

Family: `game-rewrite/v1`. Recipe id: `anixops.recipe.game.<slug>`.

## Scope

| Host | Phase | Purpose |
| ---- | ----- | ------- |

## Authority

| Probe | Outcome |
| ----- | ------- |
| Offline | |
| Reconcile | |
| Echo | |

Class: `<local|server|dual>`.

## Recon

| Field | Value |
| ----- | ----- |

## Pattern Selection

1. `<P#>` — `<primitive>`

## Module

```text
<the module file>
```

## Guard

| Key | Value | Rationale |
| --- | ----- | --------- |

## Verification

```sh
<commands and observed counters>
```

## Known Gaps

- `<adapter-required step>`

## Reversal

Load the module without `<Enable>`, or remove the module file.
````
