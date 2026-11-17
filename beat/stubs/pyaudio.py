"""Stand-in for pyaudio: BeatNet imports it at module load, offline evaluation never opens a stream."""
paInt16 = 8
class PyAudio:
    def __init__(self, *a, **k):
        raise RuntimeError("pyaudio stand-in: live input is not available in offline evaluation")
