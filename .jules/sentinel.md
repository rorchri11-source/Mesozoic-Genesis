## 2026-02-18 - Recursive Stack Exhaustion in MiniJSON
**Vulnerability:** Uncontrolled recursion in `MiniJSON::ParseValue` allowed stack exhaustion via deeply nested JSON input (DoS).
**Learning:** Custom recursive parsers often overlook depth limits, making them vulnerable to stack overflow attacks even with small input sizes (e.g., `[[[[...]]]]`).
**Prevention:** Always enforce a recursion depth limit (e.g., 256) in recursive descent parsers or use iterative parsing strategies.
