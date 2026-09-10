"use strict";
var Module = {
    canvas: document.getElementById("canvas"),
    setStatus: function (text) {
        if (text) document.getElementById("progress").textContent = "読み込み中…";
    },
    print: function (text) { console.log(text); },
    printErr: function (text) { console.error(text); },
    onAbort: function (reason) { window.konbiniFailure(String(reason)); }
};
(function () {
    const canvas = Module.canvas;
    const active = new Set();
    let ready = false;
    let failed = false;
    function suspended() {
        return document.hidden || window.matchMedia("(orientation: portrait)").matches;
    }
    function updateVisibility() {
        document.getElementById("portrait").hidden = !window.matchMedia("(orientation: portrait)").matches;
        active.clear();
        if (ready) Module._kd_suspend(suspended() ? 1 : 0);
    }
    window.konbiniFailure = function (message) {
        failed = true;
        document.getElementById("loading").hidden = true;
        document.getElementById("failure").hidden = false;
        document.getElementById("failure-text").textContent =
            "WebGL2に対応したブラウザで再読み込みしてください。 " + message;
        if (ready) Module._kd_suspend(1);
    };
    window.konbiniReady = function () {
        ready = true;
        document.getElementById("loading").hidden = true;
        updateVisibility();
    };
    function pointer(event, phase) {
        if (!ready || failed || suspended()) return;
        const rect = canvas.getBoundingClientRect();
        const x = Math.max(0, Math.min(1280, (event.clientX - rect.left) * 1280 / rect.width));
        const y = Math.max(0, Math.min(720, (event.clientY - rect.top) * 720 / rect.height));
        Module._kd_pointer(event.pointerId, phase, x, y, event.pointerType === "mouse" ? 0 : 1);
    }
    canvas.addEventListener("pointerdown", function (event) {
        if (event.button !== 0) return;
        event.preventDefault();
        canvas.focus();
        active.add(event.pointerId);
        canvas.setPointerCapture(event.pointerId);
        pointer(event, 0);
    });
    canvas.addEventListener("pointermove", function (event) {
        if (!active.has(event.pointerId)) return;
        event.preventDefault();
        pointer(event, 1);
    });
    canvas.addEventListener("pointerup", function (event) {
        if (!active.delete(event.pointerId)) return;
        event.preventDefault();
        pointer(event, 2);
        if (canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
    });
    function cancel(event) {
        if (!active.delete(event.pointerId)) return;
        pointer(event, 3);
        active.clear();
    }
    canvas.addEventListener("pointercancel", cancel);
    canvas.addEventListener("lostpointercapture", cancel);
    canvas.addEventListener("contextmenu", function (event) { event.preventDefault(); });
    canvas.addEventListener("wheel", function (event) {
        event.preventDefault();
        if (ready && !failed && !suspended()) Module._kd_wheel(-Math.sign(event.deltaY));
    }, { passive: false });
    canvas.addEventListener("webglcontextlost", function (event) {
        event.preventDefault();
        if (ready) Module._kd_context_lost();
        window.konbiniFailure("描画が中断されました。");
    });
    window.addEventListener("resize", updateVisibility);
    document.addEventListener("visibilitychange", updateVisibility);
    window.addEventListener("blur", function () {
        active.clear();
        if (ready) Module._kd_suspend(1);
    });
    window.addEventListener("focus", updateVisibility);
    document.getElementById("reload").addEventListener("click", function () { location.reload(); });
    document.getElementById("fullscreen").addEventListener("click", async function () {
        try {
            await document.getElementById("game").requestFullscreen();
            if (screen.orientation && screen.orientation.lock) {
                try { await screen.orientation.lock("landscape"); } catch (_) { /* Rotate manually. */ }
            }
        } catch (_) { /* Fullscreen is optional; the responsive canvas remains playable. */ }
    });
    if (location.protocol === "file:") window.konbiniFailure("Webサーバー経由で開いてください。");
    window.addEventListener("error", function (event) {
        if (!ready) window.konbiniFailure(event.message || "ファイルを読み込めませんでした。");
    });
    updateVisibility();
})();
