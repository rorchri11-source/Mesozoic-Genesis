## 2024-05-23 - Unbounded Recursion in Custom JSON Parser
**Vulnerability:** The custom `MiniJSON` parser in `Assets/GLTFLoader.h` lacked a recursion depth limit, allowing a Stack Overflow via deeply nested JSON structures (DoS).
**Learning:** Custom parsers for complex formats like JSON are often less robust than standard libraries. The recursive implementation was simple but dangerous.
**Prevention:** Always enforce recursion depth limits when parsing hierarchical data structures, or use iterative parsing. Prefer well-tested libraries over custom implementations for standard formats.
