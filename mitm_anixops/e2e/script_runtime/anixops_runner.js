#!/usr/bin/env node

const fs = require("node:fs");
const util = require("node:util");
const vm = require("node:vm");

function readArgs(argv) {
  const args = {};
  for (let i = 2; i < argv.length; i += 1) {
    const item = argv[i];
    if (!item.startsWith("--")) {
      throw new Error(`unexpected argument: ${item}`);
    }
    const key = item.slice(2);
    const value = argv[i + 1];
    if (value === undefined || value.startsWith("--")) {
      throw new Error(`missing value for --${key}`);
    }
    args[key] = value;
    i += 1;
  }
  return args;
}

async function main() {
  const args = readArgs(process.argv);
  const phase = args.phase || "response";
  for (const key of ["script", "url", "argument", "body"]) {
    if (!(key in args)) {
      throw new Error(`--${key} is required`);
    }
  }

  const script = fs.readFileSync(args.script, "utf8");
  const body = fs.readFileSync(args.body, "utf8");
  const logToStderr = (...items) => {
    process.stderr.write(`${items.map((item) => (typeof item === "string" ? item : util.inspect(item))).join(" ")}\n`);
  };

  let done;
  let failed;
  const resultPromise = new Promise((resolve, reject) => {
    done = resolve;
    failed = reject;
  });

  globalThis.console = {
    log: logToStderr,
    info: logToStderr,
    warn: logToStderr,
    error: logToStderr,
    debug: logToStderr,
  };
  globalThis.$anixops = {};
  globalThis.$environment = { "stash-version": "AnixOps" };
  globalThis.$script = { startTime: Date.now(), phase, type: phase };
  globalThis.$request = {
    url: args.url,
    method: args.method || "GET",
    headers: args.requestHeaders ? JSON.parse(args.requestHeaders) : {},
    body,
  };
  if (phase === "response") {
    globalThis.$response = {
      status: Number(args.status || 200),
      headers: args.responseHeaders
        ? JSON.parse(args.responseHeaders)
        : { "Content-Type": "application/json" },
      body,
    };
  }
  globalThis.$argument = args.argument;
  globalThis.$persistentStore = {
    read() {
      return null;
    },
    write() {
      return true;
    },
  };
  globalThis.$done = (value) => {
    done(value || {});
  };

  try {
    vm.runInThisContext(script, {
      filename: args.script,
      displayErrors: true,
    });
  } catch (error) {
    failed(error);
  }

  let timeoutId;
  const timeout = new Promise((_, reject) => {
    timeoutId = setTimeout(() => reject(new Error("$done timeout")), Number(args.timeoutMs || 5000));
  });
  const result = await Promise.race([resultPromise, timeout]);
  clearTimeout(timeoutId);
  process.stdout.write(`${JSON.stringify(result)}\n`);
}

main().catch((error) => {
  console.error(error && error.stack ? error.stack : String(error));
  process.exit(1);
});
