function headerValue(headers, name) {
  const wanted = name.toLowerCase();
  for (const [key, value] of Object.entries(headers || {})) {
    if (key.toLowerCase() === wanted) {
      return value;
    }
  }
  return undefined;
}

if ($script.phase === "request") {
  const payload = $request.body ? JSON.parse($request.body) : {};
  const previousArgument = $persistentStore.read("contract.argument");
  $persistentStore.write($argument, "contract.argument");
  payload.requestScript = true;
  payload.argument = $argument;
  payload.storeBefore = previousArgument;
  payload.storeAfter = $persistentStore.read("contract.argument");
  payload.staticRequestHeader = headerValue($request.headers, "X-AnixOps-Static-Request");
  $done({
    headers: {
      ...$request.headers,
      "X-AnixOps-Request-Script": "applied",
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
} else if (!$request.url.includes("timeout=1")) {
  $done({
    status: 201,
    headers: {
      "Content-Type": "application/json",
      "X-AnixOps-Script": "applied",
    },
    body: JSON.stringify({
      ok: true,
      url: $request.url,
      argument: $argument,
      persistentArgument: $persistentStore.read("contract.argument"),
      requestHeader:
        headerValue($request.headers, "X-AnixOps-Request-Script"),
      staticResponseHeader: headerValue($response.headers, "X-AnixOps-Static-Response"),
      original: JSON.parse($response.body),
    }),
  });
}
