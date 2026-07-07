# BiliUniverse Enhanced Plugin Support

Observed source:

- URL: `https://github.com/BiliUniverse/Enhanced/releases/latest/download/BiliBili.Enhanced.plugin`
- Redirect target during analysis: `v0.5.13`
- Release published by GitHub API: `2026-02-20T16:22:49Z`
- Asset digest from GitHub API: `sha256:e0eca33cd58a765283e863924030187448665e39397b1231e0dcc3db60429f52`
- `response.bundle.js` digest observed during analysis:
  `sha256:f34238dce8fc4c525a371e934382d2241a6712a366de609892c710712808f445`
- `.snippet` digest observed during analysis:
  `sha256:1f6a5799ba49a34e467cde102073880aff4fee9bf79d3e33ca60e100b1301931`
- `.sgmodule` digest observed during analysis:
  `sha256:f510220000bfe99221116038146f3af96c5f2a1f4c1f3d164c70ca3cabc02240`

The current plugin shape is:

```text
[Argument]
Name = type,default,...

[Script]
http-response <regex> requires-body=1, script-path=<url>, tag=<tag>, argument=[{Argument.Name},...]

[MitM]
hostname = app.bilibili.com, app.biliapi.net
```

The same release also ships AnixOps/Surge variants:

```text
#[rewrite_local]
<regex> url script-response-body <script-url>

#!arguments = Name:value,Other:"quoted,value"
```

The linked `response.bundle.js` accepts `$argument` as either an object or an `&`-separated string. For string input it
splits on `&`, then `=`, removes quotes, and expands dotted keys into nested objects.

Implemented in this library:

- parse `[Argument]` defaults
- allow argument overrides through `anixops_engine_set_argument_value`
- parse `.plugin` HTTP request/response script lines
- parse `.snippet` `#[rewrite_local]` script rewrite lines such as `url script-response-body`
- parse Surge-style `name = type=http-response, pattern=...` script lines
- parse `.sgmodule` `#!arguments` defaults
- ignore `.sgmodule` metadata such as `#!arguments-desc` when building defaults
- evaluate scripts by URL and phase through `anixops_script_evaluate_url`
- return `script-path`, `tag`, `requires-body`, and resolved `$argument`
- parse `[MitM] hostname`, including `%APPEND%` prefixes used by `.sgmodule`

Implemented in the E2E adapter:

- assert the pinned `.plugin`, `.snippet`, and `.sgmodule` release assets dispatch to the expected response hook
- execute the real `response.bundle.js` in a Node.js AnixOps-compatible environment
- provide `$request`, `$response`, `$argument`, `$persistentStore`, and `$done`
- assert all current plugin response hooks execute:
  `x/resource/show/tab/v2`, `x/v2/account/mine`, `x/v2/account/mine/ipad`,
  `x/v2/region/index`, and `x/v2/channel/region/list`
- assert modified tab, mine, iPad mine, region, and channel-region payloads

Still outside the C library:

- production script asset downloading/caching
- production JavaScript execution
- `$request`, `$response`, `$persistentStore`, `$done`, and platform-specific script bindings
- HTTP body buffering/decompression and response mutation
