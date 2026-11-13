# Game Rewrite Recipe Family

Family: `game-rewrite/v1`.

This family covers mini-game and mobile-game traffic that the user owns: the
account, the device, and the client are the user's own, and the observed
traffic is the user's own session. A recipe in this family observes the exact
hosts it declares and nothing else.

The family exists because game traffic has one property most rewrite recipes do
not: the value the client displays is often a projection of a server-side
ledger. Rewriting the projection is cheap. Rewriting the ledger requires
understanding which side is authoritative first. Most failed game recipes fail
at that step, not at the rewrite syntax.

## Authority Model

Every recipe in this family must classify the target before writing a single
rule.

| Class | Signature | Consequence |
| --- | --- | --- |
| `local` | Values survive an offline restart; storage keys are readable through the client cache. | Static mutation of the response or the script is enough. The cheapest recipes live here. |
| `server` | Values reset on reconnect; the settlement payload carries the value the server already decided. | Mutation must happen on the request that the server evaluates, or on the response of a server that echoes what it stored. |
| `dual` | Values survive offline but are corrected after the next reconcile call. | Both sides need a consistent story: a request mutation and a matching response projection. |

### Authority Probe

Run these three probes in order and record the result in
`#!recipe.authority`.

1. **Offline probe.** Start the game with the network blocked after first load.
   Spend one unit of the tracked currency. Restart. If the spend persisted,
   the wallet is `local` or `dual`.
2. **Reconcile probe.** Restore the network, trigger any sync action (claim a
   reward, open a shop tab, finish one level). If the value snaps back, the
   wallet is `dual`; if it snaps back only after a settlement call, it is
   `server`.
3. **Echo probe.** With a pass-through `[Script]` rule on the settlement host
   that only logs, compare the request body value with the immediately
   following response body value. A response that echoes the request value
   means the server echoes; a response that carries an independently computed
   value means the server owns the ledger.

Record all three outcomes in the recipe document. A recipe with an unprobed
authority class is not reviewable.

## Recon Loop

Recon is a loop, not a step, and it uses only supported primitives.

```text
[MITM]
hostname = <candidate hosts only>

[Script]
http-response <candidate pattern> requires-body=1, max-size=262144, timeout=3000, script-path=<passthrough.js>, tag=recipe.<id>.probe
```

`passthrough.js` reads `$response.body`, records the byte length and the top
level keys, and returns the body unchanged through `$done`, per
[script_runtime_contract](../../script_runtime_contract.md). Nothing is
rewritten during recon.

Then narrow with the no-UI runner before touching a device:

```sh
build/anixops-mitm-runner trace --plugin recipe.plugin \
  --url https://api.example.test/v1/profile --phase response
```

A candidate host that produces no matched rule is a wrong host, not a wrong
rule. Fix the scope first; a recipe that widens its scope to "make it work" is
the most common review rejection in this family.

## Strategy Selection

The family reuses the four strategies from
[../recipe-spec-v1.md](../recipe-spec-v1.md). The game-specific guidance:

- `local` authority: prefer `max` on a display field, or a storage-level write
  through the client's own cache. Keep the static rule small and readable.
- `server` authority: prefer `clamp` on the settlement request, with
  `clamp-ratio` low enough that a single transaction stays inside the target's
  plausible distribution for that level and that play duration.
- `dual` authority: mutate the request *and* the projection with the same
  factor, so a reconcile does not reveal a mismatch between the two.
- `replay`: only for a transaction that is idempotent on the server, and only
  with pacing. A replay recipe without `cooldown` is a server-side anomaly
  generator.

## Signal Hygiene

The family carries five hygiene rules. They are not moral advice; they are the
difference between a recipe that survives a week and one that trips a heuristic
in an hour.

1. **Bounded step.** One mutation moves the value by at most `clamp-ratio` of
   its current value, unless the recipe documents why a larger step is
   plausible.
2. **Plausible duration.** A settlement payload that claims a 3-second session
   with a 40-minute reward is an outlier. Keep duration and reward consistent.
3. **Pacing.** Two mutations on the same key are separated by at least
   `cooldown`. Bursts are the strongest single signal a client emits.
4. **No synthetic identity.** The recipe never rewrites account identifiers,
   device fingerprints, or signature seeds. Those belong to the client, and
   changing them turns a rewrite into a ban trigger.
5. **Reversible.** Every recipe documents the one-line change that turns it
   off: the `[Argument]` switch, or the removal of the module.

## Known Boundaries

- **Signature recomputation is `adapter-required`.** When the settlement
  request is signed, the recipe needs a script executed by the host runtime
  ([script-runtime-dependency](../../architecture/script-runtime-dependency.md)).
  The policy core dispatches the script and returns metadata; it does not run
  JavaScript. A recipe that assumes in-library script execution is wrong.
- **Binary and protobuf bodies are `adapter-required`.** JSON path and JQ steps
  do not apply. A dispatch carries `binary_body_mode` so the host can hand raw
  bytes to the script; the recipe must label that step accordingly.
- **Compression is adapter-owned.** A gzip or deflate response reaches a body
  rule only if the adapter decoded it. The Alpha shim does this within a
  bounded budget; a host that does not will silently fail open.
- **Stash projection is limited.** Stash `http.url-rewrite` supports
  reject/redirect intent only
  ([stash-url-rewrite](../../compatibility/stash-url-rewrite.md)), so a body or
  script step has no Stash projection. The recipe must say so rather than
  shipping a line Stash ignores.
- **No cross-host scope.** A recipe never lists a host it does not own, does not
  list a CDN wildcard it cannot justify, and never relies on a bare `*`.

## Files

- [patterns.md](patterns.md): the reusable pattern catalog.
- [adding-a-game.md](adding-a-game.md): the procedure and review checklist.
- [wangzhe-dalong.md](wangzhe-dalong.md): the worked example.
- [brief_zh.md](brief_zh.md): Chinese brief.
