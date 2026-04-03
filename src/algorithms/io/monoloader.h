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
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#ifndef ESSENTIA_STREAMING_MONOLOADER_H
#define ESSENTIA_STREAMING_MONOLOADER_H

#include "streamingalgorithmcomposite.h"
#include "network.h"

namespace essentia {
namespace streaming {

class MonoLoader : public AlgorithmComposite {
 protected:
  Algorithm* _audioLoader;
  Algorithm* _mixer;
  Algorithm* _resample;

  SourceProxy<AudioSample> _audio;
  bool _configured;

 public:
  MonoLoader();

  ~MonoLoader() {
    disconnect(_audioLoader->output("md5"), NOWHERE);
    disconnect(_audioLoader->output("bit_rate"), NOWHERE);
    disconnect(_audioLoader->output("codec"), NOWHERE);
    disconnect(_audioLoader->output("sampleRate"), NOWHERE);
    disconnect(_audioLoader->output("numberChannels"), NOWHERE);

    delete _audioLoader;
    delete _mixer;
    delete _resample;
  }

  void declareParameters() {
    declareParameter("filename", "the name of the file from which to read", "", Parameter::STRING);
    declareParameter("sampleRate", "the desired output sampling rate [Hz]", "(0,inf)", 44100.);
    declareParameter("downmix", "the mixing type for stereo files", "{left,right,mix}", "mix");
    declareParameter("audioStream", "audio stream index to be loaded. Other streams are no taken into account (e.g. if stream 0 is video and 1 is audio use index 0 to access it.)", "[0,inf)", 0);
    declareParameter("resampleQuality", "the resampling quality, 0 for best quality, 4 for fast linear approximation", "[0,4]", 1);
  }

  void declareProcessOrder() {
    declareProcessStep(ChainFrom(_audioLoader));
  }

  void configure();

  static const char* name;
  static const char* category;
  static const char* description;
};

} // namespace streaming
} // namespace essentia

#include "algorithm.h"
#include "audioloader.h"

namespace essentia {
namespace standard {

class MonoLoader : public Algorithm {
 protected:
  Output<std::vector<AudioSample> > _audio;

  essentia::standard::AudioLoader* _audioLoader;

  void downmix(const std::vector<StereoSample>& input,
               int numberChannels,
               const std::string& type,
               std::vector<AudioSample>& output) const;

  void linearResample(const std::vector<AudioSample>& input,
                      Real inputSampleRate,
                      Real outputSampleRate,
                      std::vector<AudioSample>& output) const;

 public:
  MonoLoader();

  ~MonoLoader();

  void declareParameters() {
    declareParameter("filename", "the name of the file from which to read", "", Parameter::STRING);
    declareParameter("sampleRate", "the desired output sampling rate [Hz]", "(0,inf)", 44100.);
    declareParameter("downmix", "the mixing type for stereo files", "{left,right,mix}", "mix");
    declareParameter("audioStream", "audio stream index to be loaded. Other streams are no taken into account (e.g. if stream 0 is video and 1 is audio use index 0 to access it.)", "[0,inf)", 0);
    declareParameter("resampleQuality", "the resampling quality, 0 for best quality, 4 for fast linear approximation", "[0,4]", 1);
  }

  void configure();
  void compute();
  void reset();

  static const char* name;
  static const char* category;
  static const char* description;
};

} // namespace standard
} // namespace essentia

#endif // ESSENTIA_STREAMING_MONOLOADER_H
