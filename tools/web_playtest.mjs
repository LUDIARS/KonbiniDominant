// Fixed browser operations and the game's existing behavior tree. No LLM.
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import assert from "node:assert/strict";

const [siteArg, evidenceArg, playwrightArg] = process.argv.slice(2);
const site = fs.realpathSync(siteArg);
const evidence = fs.realpathSync(evidenceArg);
const require = createRequire(import.meta.url);
const { chromium } = require(path.resolve(playwrightArg));
const errors = [];
const logs = [];
const checks = [];
let context;
const result = { passed: false, llmCalls: 0, checks,
    transport: "static browser HTTP responses supplied by Playwright; no network listener",
    mobileHardwareTested: false };
const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript",
    ".css": "text/css", ".wasm": "application/wasm", ".data": "application/octet-stream" };
function write(name, value) { fs.writeFileSync(path.join(evidence, name), value); }
async function records(page) {
    return page.evaluate(() => Module.FS.readFile("/playtest-report.jsonl", { encoding: "utf8" }));
}
async function tick(page) { return page.evaluate(() => Module._kd_ticks()); }
async function click(page, x, y, touch = false) {
    const box = await page.locator("#canvas").boundingBox();
    const point = { x: box.x + x * box.width / 1280, y: box.y + y * box.height / 720 };
    if (touch) await page.touchscreen.tap(point.x, point.y);
    else await page.mouse.click(point.x, point.y, { delay: 90 });
}
async function open(url) {
    const page = await context.newPage();
    page.on("pageerror", error => errors.push(String(error)));
    page.on("console", message => {
        logs.push({ type: message.type(), text: message.text() });
        if (message.type() === "error") errors.push(message.text());
    });
    await page.goto(url);
    await page.bringToFront();
    await page.waitForFunction(() => document.querySelector("#loading").hidden, null, { timeout: 60000 });
    assert.equal(await page.locator("#failure").isVisible(), false,
        await page.locator("#failure-text").textContent());
    return page;
}
try {
    context = await chromium.launchPersistentContext(path.join(evidence, "profile"), {
        channel: "msedge", headless: true, viewport: { width: 1280, height: 750 }, hasTouch: true
    });
    result.browserVersion = context.browser()?.version() || "Microsoft Edge";
    await context.route("**/*", async route => {
        const url = new URL(route.request().url());
        if (url.origin !== "http://konbini.test") return route.abort();
        if (url.pathname === "/favicon.ico") return route.fulfill({ status: 204 });
        const name = url.pathname === "/" ? "index.html" : url.pathname.slice(1);
        if (!/^(index\.(html|js|wasm|data)|browser\.js|style\.css)$/.test(name))
            return route.fulfill({ status: 404 });
        await route.fulfill({ status: 200, body: fs.readFileSync(path.join(site, name)),
            contentType: types[path.extname(name)] });
    });
    const manual = await open("http://konbini.test/");
    await manual.screenshot({ path: path.join(evidence, "chain-select.png") });
    await click(manual, 640, 288);
    await manual.waitForFunction(() => Module.FS.readFile("/playtest-report.jsonl",
        { encoding: "utf8" }).includes('"phase":1'));
    for (let i = 0; i < 10; i++) await click(manual, 561, 686);
    const before = (await records(manual)).trim().split("\n").map(JSON.parse).at(-1).stores;
    await click(manual, 640, 433);
    await manual.waitForTimeout(1200);
    const after = (await records(manual)).trim().split("\n").map(JSON.parse).at(-1).stores;
    assert.ok(after > before, "mouse placement must create a store");
    checks.push("mouse chain selection and grid placement");
    await manual.screenshot({ path: path.join(evidence, "mouse-placement.png") });
    await manual.setViewportSize({ width: 390, height: 844 });
    await manual.waitForTimeout(150);
    assert.equal(await manual.locator("#portrait").isVisible(), true);
    const pausedTick = await tick(manual);
    await manual.waitForTimeout(800);
    assert.equal(await tick(manual), pausedTick, "portrait view must pause gameplay");
    checks.push("portrait rotation pauses simulation");
    await manual.setViewportSize({ width: 844, height: 390 });
    await manual.waitForTimeout(300);
    assert.equal(await manual.locator("#portrait").isVisible(), false);
    assert.ok(await tick(manual) > pausedTick, "landscape must resume simulation");
    const touchBefore = (await records(manual)).trim().split("\n").map(JSON.parse).at(-1).stores;
    await click(manual, 767, 506, true);
    await manual.waitForTimeout(1200);
    const touchAfter = (await records(manual)).trim().split("\n").map(JSON.parse).at(-1).stores;
    assert.ok(touchAfter > touchBefore, "touch placement must create a store");
    checks.push("landscape touch placement");
    await manual.screenshot({ path: path.join(evidence, "touch-landscape.png") });
    write("manual-report.jsonl", await records(manual));
    await manual.close();

    const auto = await open("http://konbini.test/?autoplay=1&speed=100");
    await auto.waitForFunction(() => !document.querySelector("#failure").hidden ||
        Module.FS.readFile("/playtest-report.jsonl", { encoding: "utf8" }).includes('"resultPresented"'),
        null, { timeout: 180000 });
    assert.equal(await auto.locator("#failure").isVisible(), false);
    write("playtest-report.jsonl", await records(auto));
    await auto.screenshot({ path: path.join(evidence, "clear-screen.png") });
    const rows = (await records(auto)).trim().split("\n").map(JSON.parse);
    const final = rows.find(row => row.event === "resultPresented");
    assert.equal(rows[0].timeScale, 100);
    assert.equal(rows[0].autoplay, true);
    assert.equal(final?.outcome, 1);
    assert.equal(final?.endReason, 5);
    assert.ok(final.framesPresented > 0);
    result.result = final;
    result.configuredTimeScale = 100;
    checks.push("100x existing BT full campaign clear");
    assert.deepEqual(errors, [], "browser errors are not permitted");
    result.passed = true;
} catch (error) {
    result.error = String(error.stack || error);
    if (context) {
        const pages = context.pages();
        if (pages.length) await pages.at(-1).screenshot({ path: path.join(evidence, "failure.png") }).catch(() => {});
    }
} finally {
    if (context) await context.close();
    write("browser-console.json", JSON.stringify(logs, null, 2));
    write("browser-errors.json", JSON.stringify(errors, null, 2));
    write("browser-result.json", JSON.stringify(result, null, 2));
}
console.log(JSON.stringify(result));
process.exitCode = result.passed ? 0 : 1;
