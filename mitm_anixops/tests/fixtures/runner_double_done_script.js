$done({
  body: JSON.stringify({
    doubleDone: "first",
    phase: $script.phase,
    url: $request.url,
    argument: $argument,
    responseStatus: typeof $response === "undefined" ? null : $response.status,
  }),
});

$done({
  body: JSON.stringify({
    doubleDone: "second",
  }),
});
