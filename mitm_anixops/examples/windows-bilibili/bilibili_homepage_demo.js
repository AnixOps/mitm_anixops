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
.bili-video-card__cover img,
.video-card__cover img,
.feed-card img,
.bili-rank-list-video__cover img,
.floor-single-card img,
img[class*="cover"],
img[class*="Cover"],
img[class*="pic"],
img[class*="Pic"] {
  filter: brightness(0) !important;
  background: #000 !important;
}
.bili-video-card__info--tit,
.video-card__info--tit,
.feed-card .title,
.bili-rank-list-video__title,
.floor-single-card .title {
  position: relative !important;
  overflow: hidden !important;
  color: transparent !important;
  text-shadow: none !important;
}
.bili-video-card__info--tit *,
.video-card__info--tit *,
.feed-card .title *,
.bili-rank-list-video__title *,
.floor-single-card .title * {
  visibility: hidden !important;
}
.bili-video-card__info--tit::after,
.video-card__info--tit::after,
.feed-card .title::after,
.bili-rank-list-video__title::after,
.floor-single-card .title::after {
  content: "test";
  position: absolute !important;
  left: 0;
  top: 0;
  right: 0;
  color: #18191c !important;
  visibility: visible !important;
  white-space: nowrap !important;
  overflow: hidden !important;
  text-overflow: ellipsis !important;
  pointer-events: none !important;
}
</style>
<script id="${marker}-script">
(() => {
  let attempts = 0;
  function setPageTitle() {
    if (document.title !== "test") {
      document.title = "test";
    }
    const title = document.querySelector("title");
    if (title && title.textContent !== "test") {
      title.textContent = "test";
    }
    attempts += 1;
    if (attempts < 12) {
      setTimeout(setPageTitle, attempts < 4 ? 250 : 1000);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", setPageTitle, { once: true });
  } else {
    setPageTitle();
  }
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
