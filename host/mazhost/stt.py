from __future__ import annotations

from pathlib import Path
from threading import Lock

from .config import Settings


class SpeechToText:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self._models: dict[tuple[str, str, str], object] = {}
        self._lock = Lock()

    def available(self) -> bool:
        try:
            import faster_whisper  # noqa: F401
            return True
        except ImportError:
            return False

    def _get_model(self, model_name: str | None = None, device: str | None = None,
                   compute: str | None = None):
        name = model_name or self.settings.whisper_model
        selected_device = device or self.settings.whisper_device
        selected_compute = compute or self.settings.whisper_compute
        # CTranslate2 can report a CUDA device even when the runtime DLLs it
        # needs are absent. CPU int8 remains the zero-friction/3-GB-safe path.
        if selected_device == "auto":
            selected_device = "cpu"
        key = (name, selected_device, selected_compute)
        if key not in self._models:
            with self._lock:
                if key not in self._models:
                    from faster_whisper import WhisperModel
                    self._models[key] = WhisperModel(
                        name,
                        device=selected_device,
                        compute_type=selected_compute,
                    )
        return self._models[key]

    def _transcribe_with(self, path: Path, model_name: str, device: str, compute: str) -> str:
        segments, _ = self._get_model(model_name, device, compute).transcribe(
            str(path), language="en", beam_size=1, vad_filter=True
        )
        return " ".join(segment.text.strip() for segment in segments).strip()

    def transcribe(self, path: Path) -> str:
        return self._transcribe_with(
            path,
            self.settings.whisper_model,
            "cpu" if self.settings.whisper_device == "auto" else self.settings.whisper_device,
            self.settings.whisper_compute,
        )

    def transcribe_profile(self, path: Path, profile: str) -> str:
        """Teach narration presets with a conservative memory floor.

        ECO and BALANCED always use CPU int8 so screen recording/local LLM work
        cannot unexpectedly fight Whisper for a 3 GB GPU. QUALITY may use the
        globally configured Whisper device/model when the user opted into it.
        """
        choice = (profile or "balanced").lower()
        if choice == "eco":
            return self._transcribe_with(path, "base.en", "cpu", "int8")
        if choice == "quality":
            return self.transcribe(path)
        return self._transcribe_with(path, "small.en", "cpu", "int8")
