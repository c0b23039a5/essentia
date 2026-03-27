#!/usr/bin/env python3
"""
ONNX Runtime example as a TensorFlowPredict* alternative.

This mirrors the common two-stage TensorFlow flow used in Essentia examples:
  1) embedding model inference
  2) classification-head inference

Unlike TensorflowPredict/TensorflowPredict2D, this script runs ONNX models
through onnxruntime and prefers the CUDA provider when available.
"""

import argparse
import json

import numpy as np
import onnxruntime as ort
from essentia.standard import MonoLoader


def _create_session(model_path: str) -> ort.InferenceSession:
    requested = ["CUDAExecutionProvider", "CPUExecutionProvider"]
    available = ort.get_available_providers()
    providers = [provider for provider in requested if provider in available]
    if not providers:
        providers = ["CPUExecutionProvider"]
    return ort.InferenceSession(model_path, providers=providers)


def _prepare_input(array: np.ndarray, model_input: ort.NodeArg) -> np.ndarray:
    x = np.asarray(array, dtype=np.float32)
    expected_rank = len(model_input.shape)
    while x.ndim < expected_rank:
        x = np.expand_dims(x, axis=0)
    return x


def _run_single_input_model(session: ort.InferenceSession, x: np.ndarray) -> np.ndarray:
    input_meta = session.get_inputs()[0]
    prepared = _prepare_input(x, input_meta)
    outputs = session.run(None, {input_meta.name: prepared})
    return np.asarray(outputs[0])


def _load_labels(labels_path: str | None) -> list[str] | None:
    if not labels_path:
        return None
    with open(labels_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, dict) and "classes" in data:
        return [str(x) for x in data["classes"]]
    if isinstance(data, list):
        return [str(x) for x in data]
    return None


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run embedding+head ONNX inference as a TensorFlow-free alternative."
    )
    parser.add_argument("--audio", required=True, help="Path to input audio file")
    parser.add_argument("--embedding-model", required=True, help="Path to embedding ONNX model")
    parser.add_argument("--head-model", required=True, help="Path to classification head ONNX model")
    parser.add_argument("--labels-json", default=None, help="Optional labels/classes JSON")
    parser.add_argument("--sample-rate", type=int, default=16000)
    args = parser.parse_args()

    audio = MonoLoader(filename=args.audio, sampleRate=args.sample_rate, resampleQuality=4)()
    audio = np.asarray(audio, dtype=np.float32)

    embedding_session = _create_session(args.embedding_model)
    embeddings = _run_single_input_model(embedding_session, audio)

    head_session = _create_session(args.head_model)
    logits = _run_single_input_model(head_session, embeddings)

    if logits.ndim > 1:
        scores = logits.reshape(-1)
    else:
        scores = logits
    best_idx = int(np.argmax(scores))
    best_score = float(scores[best_idx])

    labels = _load_labels(args.labels_json)
    best_label = labels[best_idx] if labels and best_idx < len(labels) else str(best_idx)

    print("embedding providers:", embedding_session.get_providers())
    print("head providers:", head_session.get_providers())
    print("embeddings shape:", embeddings.shape)
    print("scores shape:", scores.shape)
    print("top-1 index:", best_idx)
    print("top-1 label:", best_label)
    print("top-1 score:", best_score)


if __name__ == "__main__":
    main()
