"""Capture/close only the native window owned by the test's child process."""
import ctypes
import time
from ctypes import wintypes


def window_api():
    api = ctypes.WinDLL("user32", use_last_error=True)
    api.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    api.IsWindowVisible.argtypes = [wintypes.HWND]
    api.GetForegroundWindow.restype = wintypes.HWND
    api.SetForegroundWindow.argtypes = [wintypes.HWND]
    api.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    api.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    api.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    return api


def game_window(process_id):
    api = window_api()
    windows = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def collect(hwnd, unused):
        owner = wintypes.DWORD()
        api.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == process_id and api.IsWindowVisible(hwnd):
            windows.append(hwnd)
        return True

    callback = callback_type(collect)
    api.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    api.EnumWindows(callback, 0)
    return windows[0] if len(windows) == 1 else None


def capture(process_id, destination):
    from PIL import ImageGrab
    api = window_api()
    api.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
    api.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    hwnd = game_window(process_id)
    if not hwnd:
        raise RuntimeError("native game window is missing or ambiguous")
    api.SetForegroundWindow(hwnd)
    time.sleep(0.1)  # Let the compositor display the requested foreground window.
    if api.GetForegroundWindow() != hwnd:
        raise RuntimeError("cannot capture an occluded game window")
    rect, point = wintypes.RECT(), wintypes.POINT()
    if not api.GetClientRect(hwnd, ctypes.byref(rect)) or not api.ClientToScreen(hwnd, ctypes.byref(point)):
        raise RuntimeError("cannot locate the native client area")
    if not (0 < rect.right <= 8192 and 0 < rect.bottom <= 8192):
        raise RuntimeError("invalid native client extent")
    picture = ImageGrab.grab(bbox=(point.x, point.y, point.x + rect.right, point.y + rect.bottom), all_screens=True)
    extrema = picture.convert("L").getextrema()
    if extrema[1] - extrema[0] < 16:
        raise RuntimeError("captured game image is blank")
    picture.save(destination)
    return {"width": rect.right, "height": rect.bottom, "humanVisualReview": False}


def close(process):
    if process.poll() is not None:
        return process.returncode
    hwnd = game_window(process.pid)
    if hwnd:
        window_api().PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE, our child only.
    try:
        return process.wait(timeout=10)
    except __import__("subprocess").TimeoutExpired:
        process.kill()
        process.wait(timeout=10)
        raise RuntimeError("native game required forced termination")
