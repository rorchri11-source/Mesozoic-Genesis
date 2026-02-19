## 2024-05-25 - Immediate Mode Feedback
**Learning:** Immediate-mode UIs lack state for "active" dragging; `hover && down` provides immediate feedback but reverts if mouse leaves element. Auto-deriving `pressedColor = color * 0.8` is a low-friction way to add tactile feel without refactoring all call sites.
**Action:** Use `hover && down` for quick feedback, but consider state tracking for complex interactions (like sliders). Always provide a default derived state.
