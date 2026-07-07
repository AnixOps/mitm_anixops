if ($script.phase === "request") {
  const previous = $persistentStore.read("runner.count");
  const next = String(Number(previous || "0") + 1);
  $persistentStore.write(next, "runner.count");
  const payload = $request.body ? JSON.parse($request.body) : {};
  payload.requestRuntime = true;
  payload.argument = $argument;
  payload.storeBefore = previous;
  payload.storeAfter = next;
  $done({
    body: JSON.stringify(payload),
  });
} else {
  const count = $persistentStore.read("runner.count");
  $persistentStore.write("response", "runner.phase");
  $done({
    status: 202,
    body: JSON.stringify({
      responseRuntime: true,
      url: $request.url,
      argument: $argument,
      storeCount: count,
      original: $response.body ? JSON.parse($response.body) : null,
    }),
  });
}
