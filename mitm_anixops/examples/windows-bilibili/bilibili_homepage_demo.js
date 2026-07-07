const headers = { ...($response.headers || {}) };
for (const key of Object.keys(headers)) {
  const normalized = key.toLowerCase();
  if (
    normalized === "content-security-policy" ||
    normalized === "content-security-policy-report-only" ||
    normalized === "content-encoding" ||
    normalized === "content-length"
  ) {
    delete headers[key];
  }
}

headers["Content-Type"] = "text/html; charset=utf-8";
headers["Cache-Control"] = "no-store";
headers["X-AnixOps-Bilibili-Demo"] = "applied";

let body = $response.body || "";
const marker = "anixops-bilibili-homepage-demo";
const injection = `
<style id="${marker}-style">
img,
picture,
video,
[class*="cover"],
[class*="Cover"],
[class*="pic"],
[class*="Pic"] {
  filter: brightness(0) !important;
  background: #000 !important;
}
</style>
<script id="${marker}-script">
(() => {
  const titleSelectors = [
    ".bili-video-card__info--tit",
    ".video-card__info--tit",
    ".feed-card .title",
    ".bili-rank-list-video__title",
    ".floor-single-card .title"
  ];
  let scheduled = false;

  function setTitle(node) {
    if (!node || node.nodeType !== Node.ELEMENT_NODE) {
      return;
    }
    if (node.children.length > 0 && node.querySelector("svg, img, picture, video")) {
      return;
    }
    const text = (node.textContent || "").trim();
    if (text && node.textContent !== "test") {
      node.textContent = "test";
    }
    if (node.hasAttribute("title") && node.getAttribute("title") !== "test") {
      node.setAttribute("title", "test");
    }
  }

  function rewrite() {
    scheduled = false;
    if (document.title !== "test") {
      document.title = "test";
    }
    const title = document.querySelector("title");
    if (title && title.textContent !== "test") {
      title.textContent = "test";
    }
    const seen = new Set();
    for (const selector of titleSelectors) {
      for (const node of document.querySelectorAll(selector)) {
        if (seen.has(node)) {
          continue;
        }
        seen.add(node);
        setTitle(node);
        if (seen.size >= 200) {
          return;
        }
      }
    }
  }

  function schedule() {
    if (scheduled) {
      return;
    }
    scheduled = true;
    requestAnimationFrame(rewrite);
  }

  rewrite();
  new MutationObserver(schedule).observe(document.body || document.documentElement, {
    childList: true,
    subtree: true
  });
  setInterval(schedule, 3000);
})();
</script>`;

if (!body.includes(marker)) {
  if (body.includes("</head>")) {
    body = body.replace("</head>", `${injection}</head>`);
  } else if (body.includes("</body>")) {
    body = body.replace("</body>", `${injection}</body>`);
  } else {
    body += injection;
  }
}

$done({
  status: $response.status || 200,
  headers,
  body
});
