#!/usr/bin/env python3

import math
import os
import tempfile
import wave

import essentia
import essentia.standard as es
import essentia.streaming as est


def _make_test_wav(path: str, sample_rate: int = 44100, seconds: float = 1.0) -> None:
    nframes = int(sample_rate * seconds)
    amplitude = 0.25
    frequency = 440.0
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        for i in range(nframes):
            sample = int(32767 * amplitude * math.sin(2.0 * math.pi * frequency * i / sample_rate))
            wav.writeframesraw(sample.to_bytes(2, byteorder="little", signed=True))


def main() -> None:
    print(f"Essentia version: {essentia.__version__}")
    with tempfile.TemporaryDirectory() as tmpdir:
        wav_path = os.path.join(tmpdir, "tone.wav")
        yaml_path = os.path.join(tmpdir, "pool.yaml")
        _make_test_wav(wav_path)

        # FFmpeg/libav + libsamplerate-backed loaders.
        audio = es.MonoLoader(filename=wav_path, sampleRate=16000)()
        assert len(audio) > 0, "MonoLoader returned empty audio."
        loader = est.MonoLoader(filename=wav_path)
        del loader

        # TagLib-backed reader should be constructible/runnable on audio files.
        metadata = es.MetadataReader(filename=wav_path)()
        assert len(metadata) >= 6, "MetadataReader returned unexpected output."

        # YAML algorithms.
        pool = essentia.Pool()
        pool.add("smoke.value", 1.23)
        es.YamlOutput(filename=yaml_path)(pool)
        loaded_pool = es.YamlInput(filename=yaml_path)()
        assert loaded_pool.descriptorNames(), "YamlInput returned an empty pool."

        # Chromaprint algorithm.
        fingerprint = es.Chromaprinter(sampleRate=16000)(audio)
        assert fingerprint, "Chromaprinter returned an empty fingerprint."

    print("Optional dependency smoke test passed.")


if __name__ == "__main__":
    main()
