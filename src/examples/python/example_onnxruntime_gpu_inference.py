#!/usr/bin/env python3
"""
Minimal ONNX Runtime inference sample with GPU preference.

This example is intended as an alternative to TensorFlow-based inference when
building Essentia without TensorFlow support.
"""

import argparse

import numpy as np
import onnxruntime as ort


def _parse_shape(shape_arg: str) -> tuple[int, ...]:
    return tuple(int(x) for x in shape_arg.split(",") if x)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run a single ONNX Runtime inference (prefers CUDA provider)."
    )
    parser.add_argument("--model", required=True, help="Path to .onnx model")
    parser.add_argument(
        "--shape",
        default="1,1,96,64",
        help="Input tensor shape as comma-separated ints (default: 1,1,96,64)",
    )
    args = parser.parse_args()

    requested_providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
    available_providers = ort.get_available_providers()
    active_providers = [p for p in requested_providers if p in available_providers]
    if not active_providers:
        active_providers = ["CPUExecutionProvider"]

    session = ort.InferenceSession(args.model, providers=active_providers)
    input_meta = session.get_inputs()[0]

    input_shape = _parse_shape(args.shape)
    x = np.random.rand(*input_shape).astype(np.float32)
    outputs = session.run(None, {input_meta.name: x})

    print("available providers:", available_providers)
    print("session providers:", session.get_providers())
    print("input name:", input_meta.name)
    print("input shape:", input_shape)
    print("num outputs:", len(outputs))
    print("output shapes:", [np.asarray(o).shape for o in outputs])


if __name__ == "__main__":
    main()
