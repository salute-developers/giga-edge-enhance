"""Inference catalog and optional Hugging Face retrieval; import performs no I/O."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
from importlib import metadata, resources
import json
from pathlib import Path, PurePosixPath
import re

from .denoiser import ProcessingError


class ModelIntegrityError(ProcessingError):
    """A selected export does not match the versioned SDK catalog."""


@dataclass(frozen=True)
class ModelDescriptor:
    name: str
    backend: str
    model_type: str
    sample_rate: int
    subfolder: str
    minimum_sdk_version: str
    maximum_sdk_major: int
    files: dict[str, str]


def available_models() -> tuple[ModelDescriptor, ...]:
    """Return the five known exports, including checksums and SDK compatibility."""
    catalog = json.loads(resources.files("giga_edge_enhance").joinpath("models.json").read_text())
    return tuple(ModelDescriptor(**entry) for entry in catalog["models"])


def _descriptor(name: str, backend: str) -> ModelDescriptor:
    for entry in available_models():
        if entry.name == name and entry.backend == backend:
            version = metadata.version("giga-edge-enhance").split(".")
            minimum = tuple(int(value) for value in entry.minimum_sdk_version.split("."))
            current = tuple(int(re.match(r"\d+", value).group()) for value in version[:3])
            if current < minimum or current[0] > entry.maximum_sdk_major:
                raise ValueError(f"{name}/{backend} is not compatible with the installed SDK")
            return entry
    raise ValueError(f"Unsupported catalog selection {name}/{backend}; fast8 supports ONNX only")


def _verify(path: Path, digest: str) -> None:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    if checksum.hexdigest() != digest:
        raise ModelIntegrityError(
            f"SHA-256 mismatch for {path.name}; use a matching model revision"
        )


def load_pretrained(
    cls,
    repo_or_path: str | Path,
    *,
    model: str = "quality48",
    backend: str = "onnx",
    revision: str | None = None,
    cache_dir: str | Path | None = None,
    local_files_only: bool = False,
    token: str | bool | None = None,
    subfolder: str | None = None,
    engine_threads: int = 1,
):
    """Load known exports from a local tree or a caller-selected Hugging Face repo.

    Hub repositories use ``<model>/<backend>/{config.yaml,model.<backend>}`` by
    default. Pin ``revision`` for reproducibility. No repository ID is invented
    by the SDK; published repository IDs are a separate release decision.
    """
    local = Path(repo_or_path).expanduser()
    if (isinstance(repo_or_path, Path) or local.is_absolute()) and not local.exists():
        raise FileNotFoundError(local)
    if local.exists() and (local.is_file() or (local / "config.yaml").is_file()):
        return cls(local, backend=backend, engine_threads=engine_threads)
    descriptor = _descriptor(model, backend)
    folder = descriptor.subfolder if subfolder is None else subfolder
    relative = PurePosixPath(folder)
    if relative.is_absolute() or ".." in relative.parts or "\\" in folder:
        raise ValueError("subfolder must be a relative POSIX path without parent traversal")
    paths = {}
    if local.is_dir():
        for filename, digest in descriptor.files.items():
            paths[filename] = local / folder / filename
            _verify(paths[filename], digest)
    else:
        try:
            from huggingface_hub import hf_hub_download
        except ImportError as error:
            raise ImportError(
                "Install giga-edge-enhance[hub] to download Hugging Face exports"
            ) from error
        for filename, digest in descriptor.files.items():
            try:
                downloaded = hf_hub_download(
                    repo_id=str(repo_or_path),
                    filename=str(relative / filename),
                    revision=revision,
                    cache_dir=cache_dir,
                    local_files_only=local_files_only,
                    token=token,
                    library_name="giga-edge-enhance",
                    library_version=metadata.version("giga-edge-enhance"),
                )
            except Exception as error:
                raise ProcessingError(f"Unable to retrieve {model}/{backend}/{filename}") from error
            paths[filename] = Path(downloaded)
            _verify(paths[filename], digest)
    # Never rewrite YAML in a shared Hub snapshot. The downloaded files may even
    # reside in distinct snapshots; the verified absolute weight path is explicit.
    config = paths["config.yaml"]
    weights = paths[f"model.{backend}"].resolve()
    yaml, replacements = re.subn(
        r"(?m)^(\s*model_path\s*:).*$",
        lambda match: match.group(1) + " " + json.dumps(str(weights)),
        config.read_text(),
        count=1,
    )
    if replacements != 1:
        raise ModelIntegrityError("Catalog configuration has no model_path")
    return cls.from_config(
        yaml, base_directory=config.parent, backend=backend, engine_threads=engine_threads
    )
