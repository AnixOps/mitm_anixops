if ($script.phase === "request") {
  const payload = $request.body ? JSON.parse($request.body) : {};
  payload.requestScript = true;
  payload.argument = $argument;
  $done({
    headers: {
      ...$request.headers,
      "X-AnixOps-Request-Script": "applied",
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
} else {
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
      requestHeader:
        $request.headers["X-AnixOps-Request-Script"] ||
        $request.headers["X-Anixops-Request-Script"] ||
        $request.headers["x-anixops-request-script"],
      original: JSON.parse($response.body),
    }),
  });
}
