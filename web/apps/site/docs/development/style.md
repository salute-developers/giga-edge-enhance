---
title: Repository style
---

# Repository style

C and C++ changes follow `STYLE.md` and `.clang-format`. Internal functions use `snake_case`, types
use `PascalCase`, and data members use a trailing underscore. Existing public C and C++ names are
compatibility constraints.

```bash
git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- spench
```

Web changes use the workspace's TypeScript and ESLint configuration and should preserve accessible
HTML semantics.
