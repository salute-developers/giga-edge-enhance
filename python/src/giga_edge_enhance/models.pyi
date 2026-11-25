from dataclasses import dataclass
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

def available_models() -> tuple[ModelDescriptor, ...]: ...
