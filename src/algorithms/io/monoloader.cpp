/*
 * Copyright (C) 2006-2021  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include "monoloader.h"
#include "algorithmfactory.h"
#include <cmath>

using namespace std;

namespace essentia {
namespace streaming {

const char* MonoLoader::name = essentia::standard::MonoLoader::name;
const char* MonoLoader::category = essentia::standard::MonoLoader::category;
const char* MonoLoader::description = essentia::standard::MonoLoader::description;

MonoLoader::MonoLoader() : AlgorithmComposite(),
                           _audioLoader(0), _mixer(0), _resample(0), _configured(false) {
  declareOutput(_audio, "audio", "the mono audio signal");

  AlgorithmFactory& factory = AlgorithmFactory::instance();

  _audioLoader = factory.create("AudioLoader");
  _mixer       = factory.create("MonoMixer");
  _resample    = factory.create("Resample");

  _audioLoader->output("audio") >> _mixer->input("audio");
  _mixer->output("audio")       >> _resample->input("signal");

  _audioLoader->output("numberChannels") >> NOWHERE;
  _audioLoader->output("sampleRate")     >> NOWHERE;
  _audioLoader->output("md5")            >> NOWHERE;
  _audioLoader->output("bit_rate")       >> NOWHERE;
  _audioLoader->output("codec")          >> NOWHERE;

  attach(_resample->output("signal"), _audio);
}

void MonoLoader::configure() {
  Parameter filename = parameter("filename");
  if (!filename.isConfigured()) return;

  _audioLoader->configure("filename", filename,
                          "computeMD5", false,
                          INHERIT("audioStream"));

  // streaming版は今回の修正対象ではない
}

} // namespace streaming
} // namespace essentia


namespace essentia {
namespace standard {

const char* MonoLoader::name = "MonoLoader";
const char* MonoLoader::category = "Input/output";
const char* MonoLoader::description = DOC("This algorithm loads the raw audio data from an audio file and downmixes it to mono. Audio is resampled in case the given sampling rate does not match the sampling rate of the input signal.\n"
"\n"
"This implementation uses standard::AudioLoader internally.");

MonoLoader::MonoLoader() : _audioLoader(new essentia::standard::AudioLoader()) {
  declareOutput(_audio, "audio", "the audio signal");
}

MonoLoader::~MonoLoader() {
  delete _audioLoader;
}

void MonoLoader::configure() {
  if (!parameter("filename").isConfigured()) return;

  _audioLoader->configure("filename", parameter("filename"),
                          "computeMD5", false,
                          "audioStream", parameter("audioStream"));
}

void MonoLoader::downmix(const vector<StereoSample>& input,
                         int numberChannels,
                         const string& type,
                         vector<AudioSample>& output) const {
  output.resize(input.size());

  if (numberChannels == 1) {
    for (size_t i = 0; i < input.size(); ++i) {
      output[i] = input[i].left();
    }
    return;
  }

  if (type == "left") {
    for (size_t i = 0; i < input.size(); ++i) {
      output[i] = input[i].left();
    }
    return;
  }

  if (type == "right") {
    for (size_t i = 0; i < input.size(); ++i) {
      output[i] = input[i].right();
    }
    return;
  }

  for (size_t i = 0; i < input.size(); ++i) {
    output[i] = 0.5f * (input[i].left() + input[i].right());
  }
}

void MonoLoader::linearResample(const vector<AudioSample>& input,
                                Real inputSampleRate,
                                Real outputSampleRate,
                                vector<AudioSample>& output) const {
  if (input.empty()) {
    output.clear();
    return;
  }

  if (inputSampleRate <= 0 || outputSampleRate <= 0) {
    throw EssentiaException("MonoLoader: sample rates must be greater than 0");
  }

  if (inputSampleRate == outputSampleRate) {
    output = input;
    return;
  }

  const double ratio = (double)outputSampleRate / (double)inputSampleRate;
  size_t outputSize = (size_t)std::llround((double)input.size() * ratio);
  if (outputSize == 0) outputSize = 1;

  output.resize(outputSize);

  for (size_t j = 0; j < outputSize; ++j) {
    double srcPos = (double)j / ratio;
    size_t i0 = (size_t)std::floor(srcPos);

    if (i0 >= input.size() - 1) {
      output[j] = input.back();
      continue;
    }

    size_t i1 = i0 + 1;
    double frac = srcPos - (double)i0;
    output[j] = (AudioSample)((1.0 - frac) * input[i0] + frac * input[i1]);
  }
}

void MonoLoader::compute() {
  if (!parameter("filename").isConfigured()) {
    throw EssentiaException("MonoLoader: Trying to call compute() on a MonoLoader algo which hasn't been correctly configured.");
  }

  // nested standard::AudioLoader が compute() できるように一度バインドはする
  vector<StereoSample> nestedAudio;
  Real nestedSampleRate = 0.0;
  int nestedChannels = 0;
  string nestedMD5;
  int nestedBitRate = 0;
  string nestedCodec;

  _audioLoader->output("audio").set(nestedAudio);
  _audioLoader->output("sampleRate").set(nestedSampleRate);
  _audioLoader->output("numberChannels").set(nestedChannels);
  _audioLoader->output("md5").set(nestedMD5);
  _audioLoader->output("bit_rate").set(nestedBitRate);
  _audioLoader->output("codec").set(nestedCodec);

  _audioLoader->compute();

  const vector<StereoSample>& stereoAudio = _audioLoader->lastAudio();
  Real inputSampleRate = _audioLoader->lastSampleRate();
  int numberChannels = _audioLoader->lastNumberChannels();

  vector<AudioSample> monoAudio;
  const string downmixType = parameter("downmix").toLower();
  downmix(stereoAudio, numberChannels, downmixType, monoAudio);

  vector<AudioSample>& output = _audio.get();
  const Real targetSampleRate = parameter("sampleRate").toReal();

  linearResample(monoAudio, inputSampleRate, targetSampleRate, output);
}

void MonoLoader::reset() {
  if (_audioLoader) {
    _audioLoader->reset();
  }
}

} // namespace standard
} // namespace essentia
