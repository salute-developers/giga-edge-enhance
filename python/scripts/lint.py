"""Static Python checks over the authored SDK surface, without native imports."""

from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    for arguments in (
        ["black", "--check", "python"],
        [
            "ruff",
            "check",
            "python",
            "tools/infra/python_workflow.py",
            "tools/infra/tests/test_python_workflow.py",
        ],
    ):
        subprocess.run([sys.executable, "-m", *arguments], cwd=ROOT, check=True)
    environment = os.environ.copy()
    environment["MYPYPATH"] = str(ROOT / "python/src")
    subprocess.run(
        [
            sys.executable,
            "-m",
            "mypy",
            "--strict",
            "--follow-imports=silent",
            "--cache-dir",
            str(ROOT / "out/python/mypy"),
            "python/tests/typing_consumer.py",
        ],
        cwd=ROOT,
        env=environment,
        check=True,
    )


if __name__ == "__main__":
    main()
