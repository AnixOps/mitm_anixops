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
    "title",
    ".bili-video-card__info--tit",
    ".video-card__info--tit",
    ".feed-card .title",
    "a[title]",
    "[class*='title']",
    "[class*='Title']"
  ];
  const coverSelectors = [
    "img",
    "picture",
    "video",
    "[class*='cover']",
    "[class*='Cover']",
    "[class*='pic']",
    "[class*='Pic']"
  ];

  function rewrite() {
    document.title = "test";
    for (const selector of titleSelectors) {
      document.querySelectorAll(selector).forEach((node) => {
        if (node.tagName === "TITLE") {
          node.textContent = "test";
          return;
        }
        if (node.children.length > 0 && node.querySelector("svg, img, video")) {
          return;
        }
        if ((node.textContent || "").trim().length > 0) {
          node.textContent = "test";
        }
        if (node.hasAttribute("title")) {
          node.setAttribute("title", "test");
        }
      });
    }
    for (const selector of coverSelectors) {
      document.querySelectorAll(selector).forEach((node) => {
        node.style.setProperty("filter", "brightness(0)", "important");
        node.style.setProperty("background", "#000", "important");
      });
    }
  }

  rewrite();
  new MutationObserver(rewrite).observe(document.documentElement, {
    childList: true,
    subtree: true
  });
  setInterval(rewrite, 1000);
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
