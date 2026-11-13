# Adding A Recipe

This is the procedure for writing one recipe module against the format in
[recipe-spec-v1.md](recipe-spec-v1.md). It is domain-neutral: the same eight
steps apply to any client whose displayed state is a projection of remote state,
including games, progression trackers, and account dashboards.

The module itself does not live in this repository. See
[external-examples.md](external-examples.md) for where it does live and what an
external module repository must satisfy.

---

## Step 1: Scope

Decide the hosts first. Write them as exact hosts.

```text
api.example.test
*.assets.example.test     # only with a stated reason
```

Rules for scope:

- one host per line in both `#!recipe.scope` and `[MITM] hostname`;
- a wildcard needs a justification sentence in the module document;
- a host you cannot name belongs to a different recipe;
- the recipe scope and the `[MITM]` list must be identical sets, not subsets.

Why: the policy core only decrypts hosts the adapter was told to decrypt, per
[certificate-lifecycle](../architecture/certificate-lifecycle.md). A scope
larger than the target means the module observes traffic it has no business
observing, and every reviewer reads that as a defect.

## Step 2: Authority

Determine which side decides the value you intend to change, before writing a
rule. Run three probes and record the outcomes.

| Probe | Procedure | Reads as |
| --- | --- | --- |
| Offline probe | Change the value with the network blocked after first load, restart the client, observe whether the change survived | client-decided state |
| Reconcile probe | Restore the network, trigger a sync action, observe whether the value is corrected | server-decided state |
| Echo probe | Compare the request body value with the immediately following response value | echo means the server stores what it is told; a recomputed value means the server owns the ledger |

Write the conclusion to `#!recipe.authority` as `local`, `server`, or `dual`.

Why: the probe result decides whether the module mutates the response, the
request, or both. A module written against the wrong side either does nothing or
produces a transient state that the next reconcile corrects.

## Step 3: Recon

Install a pass-through script on the candidate host and record the payload
shape. Keep the budget small so recon cannot become a body-buffering problem.

```text
[Script]
http-response ^https?:\/\/api\.example\.test\/v1\/ requires-body=1, max-size=65536, timeout=3000, script-path=https://modules.example.com/recipe/recon.js, tag=recipe.recon
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

Record in the module document: host, path, method, phase, top-level keys, and
whether the payload is JSON. `$done({})` writes nothing back, so recon cannot
change behavior.

## Step 4: Select Steps

Choose the smallest set of mutation shapes that reaches the outcome.

- One step is normal.
- Two steps is normal when the value is decided on both sides.
- Three steps usually means the target is two recipes, or the authority
  classification is wrong.

Write the selection into the module document as a numbered list, each line
naming the shape and the primitive.

## Step 5: Parameterize

Every number a reader may want to change belongs in `[Argument]`.

```text
[Argument]
Enable = switch,true,tag=[总开关],desc=Disable the module without unloading it.
Step.Ratio = input,0.25,tag=[Step],desc=Maximum relative change per mutation.
Ceiling = input,5000,tag=[Ceiling],desc=Absolute value the module never exceeds.
```

and is referenced from the rule that uses it:

```text
[Script]
http-response ^https?:\/\/api\.example\.test\/v1\/settle requires-body=1, max-size=262144, timeout=3000, script-path=https://modules.example.com/recipe/settle.js, tag=recipe.settle, argument=[{Enable},{Step.Ratio},{Ceiling}]
```

Why: a hard-coded value makes the module unauditable and makes the `Enable`
switch impossible, which breaks the reversibility requirement in
[recipe-spec-v1.md](recipe-spec-v1.md#conformance-checklist).

## Step 6: Guard

Apply the strategy guard from
[recipe-spec-v1.md](recipe-spec-v1.md#metadata-keys).

```text
#!recipe.strategy = clamp
#!recipe.guard = clamp-ratio=0.25, cooldown=90, monotonic=1, host-allowlist=1, max-size=262144
```

Why: a bounded step plus pacing keeps the module inside the distribution the
target already accepts for that input. An unbounded step is the single most
common reason a module stops working after one run.

## Step 7: Verify

```sh
# parse, and count the tolerated metadata diagnostics
build/anixops-mitm-runner scan recipe.plugin

# phase and host matching, positive
build/anixops-mitm-runner trace --plugin recipe.plugin \
  --url https://api.example.test/v1/settle --phase response

# phase and host matching, negative: a request trace must not match a response-only rule set
build/anixops-mitm-runner trace --plugin recipe.plugin \
  --url https://api.example.test/v1/settle --phase request

# out-of-scope host must produce zero matches
build/anixops-mitm-runner trace --plugin recipe.plugin \
  --url https://api.other.test/v1/settle --phase response

# determinism: the same input body must produce the same output body twice
build/anixops-mitm-runner replay --plugin recipe.plugin --fixture recipe.tsv
```

Record the observed counters in the module document. A negative trace that
matches is a phase bug in the module, not in the library.

## Step 8: Document

One document per module, in the module's own repository, covering: scope table,
authority table, recon table, step list, the module file, the guard table, the
verification commands with observed counters, known gaps, and the reversal
instruction.

---

## Review Checklist

A reviewer accepts a new module when every line holds:

- [ ] Scope is exact, justified, and identical in metadata and `[MITM]`.
- [ ] Authority is probed, not assumed, and the probe outcomes are recorded.
- [ ] Step selection is minimal and each step names its primitive.
- [ ] Every reader-tunable number is an `[Argument]` reference.
- [ ] Strategy guard is present and matches the strategy.
- [ ] `runner scan`, positive `trace`, negative `trace`, out-of-scope `trace`,
      and `replay` all pass.
- [ ] Every `adapter-required` step is labeled, including optional JQ builds,
      script execution, binary bodies, and compression.
- [ ] Reversibility is one switch or one module removal.
- [ ] The document states known gaps.
- [ ] No host outside the declared scope appears anywhere in the file.
- [ ] No step depends on an ordering accident between two body rules.

Common defects, in the order reviewers find them: scope widening, competing body
rules on one path, a hard-coded value, an unlabeled `adapter-required` step, and
an unpaced repeated request.

---

## Promotion To Compatibility Fixtures

A module snippet needs no contract change while it uses grammar the existing
fixtures already prove. If it introduces a grammar shape that no fixture
exercises yet, the next pull request to this repository does all of the
following together:

1. add `tests/fixtures/<Ecosystem>.<Name>.<ext>` and a malformed counterpart;
2. register `config/<name>_fixture_*` tests in `tests/test_config.c`;
3. add a paragraph to the matching `docs/compatibility/*.md` contract;
4. update the matrix row;
5. run `sh scripts/check.sh`.

That is the path from a module shape to a library capability. Until that pull
request lands, the module must use only grammar the existing fixtures already
prove, per
[recipe-spec-v1.md](recipe-spec-v1.md#primitive-mapping).

---

## Module Template

````text
#!name = <name>
#!desc = <one line>
#!recipe = recipe/v1
#!recipe.id = <reverse-dns id>
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
<one supported rewrite line>
````
