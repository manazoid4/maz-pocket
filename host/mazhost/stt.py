from __future__ import annotations

from pathlib import Path
from threading import Lock

from .config import Settings


class SpeechToText:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self._model = None
        self._lock = Lock()

    def available(self) -> bool:
        try:
            import faster_whisper  # noqa: F401
            return True
        except ImportError:
            return False

    def _get_model(self):
        if self._model is None:
            with self._lock:
                if self._model is None:
                    from faster_whisper import WhisperModel

                    # CTranslate2 can report a CUDA device even when the CUDA
                    # runtime DLLs it needs are absent. CPU int8 is the reliable
                    # zero-setup default; users can still opt into cuda.
                    device = "cpu" if self.settings.whisper_device == "auto" else self.settings.whisper_device
                    self._model = WhisperModel(
                        self.settings.whisper_model,
                        device=device,
                        compute_type=self.settings.whisper_compute,
                    )
        return self._model

    def transcribe(self, path: Path) -> str:
        segments, _ = self._get_model().transcribe(
            str(path), language="en", beam_size=1, vad_filter=True
        )
        return " ".join(segment.text.strip() for segment in segments).strip()
