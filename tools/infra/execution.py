"""Host and container process execution."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

from .config import BAKE_FILE, ROOT
from .identity import container_image_name


def ensure_container_image(container_stage: str) -> str:
    """Build a toolchain only when its Dockerfile inputs changed."""
    if shutil.which("docker") is None:
        raise SystemExit("Docker is required for this target. Use --host for a local toolchain.")
    if subprocess.run(["docker", "info"], stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL).returncode != 0:
        raise SystemExit("Docker is installed but its daemon is unavailable. Start Docker or use --host.")

    image = container_image_name(container_stage)
    if subprocess.run(["docker", "image", "inspect", image], stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL).returncode == 0:
        return image

    remote_image = os.environ.get(
        f"GEE_CONTAINER_IMAGE_{container_stage.upper().replace('-', '_')}")
    if remote_image:
        print(f"Restoring {container_stage} toolchain image: {remote_image}")
        if subprocess.run(["docker", "pull", remote_image]).returncode == 0:
            subprocess.run(["docker", "tag", remote_image, image], check=True)
            return image
        print("Remote toolchain image is unavailable; building it locally.")

    build_command = [
        "docker", "buildx", "bake", "--file", str(BAKE_FILE),
        "--set", f"{container_stage}.tags={image}",
        "--set", f"{container_stage}.output=type=docker",
        container_stage
    ]
    subprocess.run(build_command, cwd=ROOT, check=True)
    return image


def run(command: list[str], *, container_stage: str | None = None,
        environment: dict[str, str] | None = None, workdir: Path = ROOT,
        isolate_web_node_modules: bool = False,
        devices: tuple[str, ...] = (), interactive: bool = False,
        mount_host_home: bool = False,
        additional_mounts: tuple[tuple[Path, str], ...] = ()) -> None:
    if interactive and (not sys.stdin.isatty() or not sys.stdout.isatty()):
        raise SystemExit("The shell action requires an interactive terminal.")
    if container_stage is None:
        process_environment = os.environ.copy()
        process_environment.update(environment or {})
        add_github_git_auth(process_environment)
        subprocess.run(command, cwd=workdir, check=True, env=process_environment)
        return

    image = ensure_container_image(container_stage)

    cache_home = ROOT / "out" / "cache" / "container-home"
    cache_home.mkdir(parents=True, exist_ok=True)
    container_home = "/workspace/out/cache/container-home"
    container_command = [
        "docker", "run", "--rm", "--init", "--platform", "linux/amd64",
        "--user", f"{os.getuid()}:{os.getgid()}",
        "--volume", f"{ROOT}:/workspace",
        "--workdir", f"/workspace/{workdir.relative_to(ROOT)}",
    ]
    for source, destination in additional_mounts:
        container_command.extend(("--volume", f"{source}:{destination}"))
    if interactive:
        container_command.extend(("--interactive", "--tty"))
    if mount_host_home:
        host_home = Path.home()
        container_home = "/host-home"
        print(f"Mounting host home {host_home} at {container_home}.")
        container_command.extend(("--volume", f"{host_home}:{container_home}"))
    for device in devices:
        if not Path(device).exists():
            raise SystemExit(f"Required container device is unavailable: {device}")
        container_command.extend(("--device", device))
    if isolate_web_node_modules:
        dependency_hash = hashlib.sha256(
            (ROOT / "web/package-lock.json").read_bytes()
            + (ROOT / "web/.nvmrc").read_bytes()
        ).hexdigest()[:12]
        cache_root = ROOT / "out/cache/container-node-modules/linux-amd64" / dependency_hash
        for cache_name, container_path in (
            ("web", "/workspace/web/node_modules"),
            ("site", "/workspace/web/apps/site/node_modules"),
        ):
            cache_path = cache_root / cache_name
            cache_path.mkdir(parents=True, exist_ok=True)
            container_command.extend(("--volume", f"{cache_path}:{container_path}"))
    container_command.extend((
        "--env", f"HOME={container_home}",
        "--env", "GRADLE_USER_HOME=/workspace/out/cache/gradle",
        "--env", "npm_config_cache=/workspace/out/cache/npm",
    ))
    for name, value in (environment or {}).items():
        container_command.extend(("--env", f"{name}={value}"))
    container_git_environment = {
        "GIT_CONFIG_COUNT": "1",
        "GIT_CONFIG_KEY_0": "safe.directory",
        "GIT_CONFIG_VALUE_0": "*",
    }
    if header := os.environ.get("GEE_GITHUB_AUTH_HEADER"):
        container_git_environment["GEE_GITHUB_AUTH_HEADER"] = header
    add_github_git_auth(container_git_environment)
    container_environment = os.environ.copy()
    container_environment.pop("GEE_GITHUB_AUTH_HEADER", None)
    container_environment.update(container_git_environment)
    for name in sorted(container_git_environment):
        container_command.extend(("--env", name))
    container_command.extend((
        image,
        "sh",
        "-c",
        "exec \"$@\"",
        "sh",
        *command,
    ))
    process = subprocess.run(
        container_command, check=not interactive, env=container_environment)
    if interactive and process.returncode:
        raise SystemExit(process.returncode)


def add_github_git_auth(environment: dict[str, str]) -> None:
    """Add ephemeral GitHub HTTPS authentication without persisting credentials."""
    header = environment.pop("GEE_GITHUB_AUTH_HEADER", "")
    if not header:
        return
    try:
        index = int(environment.get("GIT_CONFIG_COUNT", "0"))
    except ValueError as error:
        raise SystemExit("GIT_CONFIG_COUNT must be an integer.") from error
    environment[f"GIT_CONFIG_KEY_{index}"] = "http.https://github.com/.extraheader"
    environment[f"GIT_CONFIG_VALUE_{index}"] = header
    environment["GIT_CONFIG_COUNT"] = str(index + 1)


def compiler_cache_environment(container: bool) -> dict[str, str]:
    cache_dir = ROOT / "out/cache/compiler"
    cache_dir.mkdir(parents=True, exist_ok=True)
    if container:
        sccache_path = "/workspace/out/cache/compiler/sccache"
        ccache_path = "/workspace/out/cache/compiler/ccache"
        source_path = "/workspace"
    else:
        sccache_path = str(cache_dir / "sccache")
        ccache_path = str(cache_dir / "ccache")
        source_path = str(ROOT)
    return {
        "SCCACHE_DIR": sccache_path,
        "SCCACHE_CACHE_SIZE": "5G",
        "CCACHE_DIR": ccache_path,
        "CCACHE_BASEDIR": source_path,
        "CCACHE_NOHASHDIR": "true",
        "CCACHE_MAXSIZE": "5G",
    }


def print_compiler_cache_stats(container_stage: str | None,
                               environment: dict[str, str]) -> None:
    """Show cache counters without making cache availability fatal."""
    command = ["ccache", "--show-stats"] if container_stage is not None else None
    if command is None:
        if shutil.which("sccache"):
            command = ["sccache", "--show-stats"]
        elif shutil.which("ccache"):
            command = ["ccache", "--show-stats"]
    if command is None:
        return
    print("Compiler cache statistics:")
    try:
        run(command, container_stage=container_stage, environment=environment)
    except subprocess.CalledProcessError:
        print("Compiler cache statistics are unavailable for this build.")
