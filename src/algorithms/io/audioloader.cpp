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

#include "audioloader.h"
#include "algorithmfactory.h"
#include <iomanip>  // setw()

using namespace std;

namespace essentia {
namespace streaming {

namespace {

int channelCountFromLayout(const AVChannelLayout& layout) {
  return layout.nb_channels;
}

int guessChannelCount(const AVCodecContext* audioCtx,
                      const AVCodecParameters* codecParams,
                      const AVFrame* decodedFrame) {
  int nChannels = 0;

  if (nChannels <= 0 && decodedFrame) {
    nChannels = channelCountFromLayout(decodedFrame->ch_layout);
  }
  if (nChannels <= 0 && audioCtx) {
    nChannels = channelCountFromLayout(audioCtx->ch_layout);
  }
  if (nChannels <= 0 && codecParams) {
    nChannels = channelCountFromLayout(codecParams->ch_layout);
  }
  return nChannels;
}

Real guessSampleRate(const AVCodecContext* audioCtx,
                     const AVCodecParameters* codecParams,
                     const AVFrame* decodedFrame,
                     const AVStream* stream) {
  int sampleRate = 0;

  if (sampleRate <= 0 && decodedFrame) {
    sampleRate = decodedFrame->sample_rate;
  }
  if (sampleRate <= 0 && audioCtx) {
    sampleRate = audioCtx->sample_rate;
  }
  if (sampleRate <= 0 && codecParams) {
    sampleRate = codecParams->sample_rate;
  }
  if (sampleRate <= 0 && stream && stream->time_base.num == 1 && stream->time_base.den > 0) {
    sampleRate = stream->time_base.den;
  }
  return (Real)sampleRate;
}

int guessBitRate(const AVCodecContext* audioCtx,
                 const AVCodecParameters* codecParams,
                 const AVFormatContext* demuxCtx) {
  int bitRate = 0;

  if (bitRate <= 0 && audioCtx) {
    bitRate = audioCtx->bit_rate;
  }
  if (bitRate <= 0 && codecParams) {
    bitRate = codecParams->bit_rate;
  }
  if (bitRate <= 0 && demuxCtx) {
    bitRate = demuxCtx->bit_rate;
  }
  if (bitRate < 0) {
    bitRate = 0;
  }
  return bitRate;
}

std::string guessCodecName(const AVCodec* audioCodec) {
  if (audioCodec && audioCodec->name) {
    return std::string(audioCodec->name);
  }
  return "";
}

} // namespace

const char* AudioLoader::name = essentia::standard::AudioLoader::name;
const char* AudioLoader::category = essentia::standard::AudioLoader::category;
const char* AudioLoader::description = essentia::standard::AudioLoader::description;


AudioLoader::~AudioLoader() {
  closeAudioFile();

  av_freep(&_buffer);
  av_freep(&_md5Encoded);
  av_freep(&_decodedFrame);
}

void AudioLoader::configure() {
  av_log_set_level(AV_LOG_QUIET);
  _computeMD5 = parameter("computeMD5").toBool();
  _selectedStream = parameter("audioStream").toInt();
  reset();
}


void AudioLoader::openAudioFile(const string& filename) {
  E_DEBUG(EAlgorithm, "AudioLoader: opening file: " << filename);

  int errnum;
  if ((errnum = avformat_open_input(&_demuxCtx, filename.c_str(), NULL, NULL)) != 0) {
    char errorstr[128];
    string error = "Unknown error";
    if (av_strerror(errnum, errorstr, 128) == 0) error = errorstr;
    throw EssentiaException("AudioLoader: Could not open file \"", filename, "\", error = ", error);
  }

  if ((errnum = avformat_find_stream_info(_demuxCtx, NULL)) < 0) {
    char errorstr[128];
    string error = "Unknown error";
    if (av_strerror(errnum, errorstr, 128) == 0) error = errorstr;
    avformat_close_input(&_demuxCtx);
    _demuxCtx = 0;
    throw EssentiaException("AudioLoader: Could not find stream information, error = ", error);
  }

  _streams.clear();
  for (int i = 0; i < (int)_demuxCtx->nb_streams; i++) {
    const AVCodecParameters* codecParams = _demuxCtx->streams[i]->codecpar;
    if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO) {
      _streams.push_back(i);
    }
  }
  int nAudioStreams = (int)_streams.size();

  if (nAudioStreams == 0) {
    avformat_close_input(&_demuxCtx);
    _demuxCtx = 0;
    throw EssentiaException("AudioLoader ERROR: found 0 streams in the file, expecting one or more audio streams");
  }

  if (_selectedStream >= nAudioStreams) {
    avformat_close_input(&_demuxCtx);
    _demuxCtx = 0;
    throw EssentiaException("AudioLoader ERROR: 'audioStream' parameter set to ", _selectedStream,
                            ". It should be smaller than the audio streams count, ", nAudioStreams);
  }

  _streamIdx = _streams[_selectedStream];

  const AVCodecParameters* codecParams = _demuxCtx->streams[_streamIdx]->codecpar;
  _audioCodec = avcodec_find_decoder(codecParams->codec_id);

  if (!_audioCodec) {
    throw EssentiaException("AudioLoader: Unsupported codec!");
  }

  _audioCtx = avcodec_alloc_context3(_audioCodec);
  if (!_audioCtx) {
    throw EssentiaException("AudioLoader: Could not allocate codec context");
  }

  if (avcodec_parameters_to_context(_audioCtx, codecParams) < 0) {
    avcodec_free_context(&_audioCtx);
    throw EssentiaException("AudioLoader: Could not copy codec parameters");
  }

  if (avcodec_open2(_audioCtx, _audioCodec, NULL) < 0) {
    avcodec_free_context(&_audioCtx);
    throw EssentiaException("AudioLoader: Unable to instantiate codec...");
  }

  AVChannelLayout layout;
  if (_audioCtx->ch_layout.nb_channels > 0) {
    layout = _audioCtx->ch_layout;
  }
  else {
    int fallbackChannels = guessChannelCount(_audioCtx, codecParams, 0);
    if (fallbackChannels <= 0) {
      avcodec_free_context(&_audioCtx);
      throw EssentiaException("AudioLoader: Could not determine channel count from stream metadata");
    }
    av_channel_layout_default(&layout, fallbackChannels);
  }

  E_DEBUG(EAlgorithm, "AudioLoader: using sample format conversion from libswresample");
  _convertCtxAv = swr_alloc();

  av_opt_set_chlayout(_convertCtxAv, "in_chlayout", &layout, 0);
  av_opt_set_chlayout(_convertCtxAv, "out_chlayout", &layout, 0);
  av_opt_set_int(_convertCtxAv, "in_sample_rate", _audioCtx->sample_rate, 0);
  av_opt_set_int(_convertCtxAv, "out_sample_rate", _audioCtx->sample_rate, 0);
  av_opt_set_int(_convertCtxAv, "in_sample_fmt", _audioCtx->sample_fmt, 0);
  av_opt_set_int(_convertCtxAv, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

  if (swr_init(_convertCtxAv) < 0) {
    throw EssentiaException("AudioLoader: Could not initialize swresample context");
  }

  av_init_packet(&_packet);

  _decodedFrame = av_frame_alloc();
  if (!_decodedFrame) {
    throw EssentiaException("AudioLoader: Could not allocate audio frame");
  }

  av_md5_init(_md5Encoded);
}


void AudioLoader::closeAudioFile() {
  if (!_demuxCtx) {
    return;
  }

  if (_convertCtxAv) {
    swr_close(_convertCtxAv);
    swr_free(&_convertCtxAv);
  }

  if (_audioCtx) {
    avcodec_free_context(&_audioCtx);
  }

  if (_demuxCtx) avformat_close_input(&_demuxCtx);

  av_packet_unref(&_packet);
  _demuxCtx = 0;
  _audioCtx = 0;
  _streams.clear();
}


void AudioLoader::writeChannelsSampleRateInfo(int nChannels, Real sampleRate) {
  if (nChannels > 2) {
    throw EssentiaException("AudioLoader: could not load audio. Audio file has more than 2 channels.");
  }
  if (sampleRate <= 0) {
    throw EssentiaException("AudioLoader: could not load audio. Audio sampling rate must be greater than 0.");
  }

  _nChannels = nChannels;
  _channels.firstToken() = nChannels;
  _sampleRate.firstToken() = sampleRate;
}


void AudioLoader::writeCodecInfo(const std::string& codec, int bit_rate) {
  _codec.firstToken() = codec;
  _bit_rate.firstToken() = bit_rate;
}

void AudioLoader::writeMD5Info(const std::string& md5) {
  _md5.firstToken() = md5;
}


string uint8_t_to_hex(uint8_t* input, int size) {
  ostringstream result;
  for (int i = 0; i < size; ++i) {
    result << setw(2) << setfill('0') << hex << (int)input[i];
  }
  return result.str();
}


AlgorithmStatus AudioLoader::process() {
  if (!parameter("filename").isConfigured()) {
    throw EssentiaException("AudioLoader: Trying to call process() on an AudioLoader algo which hasn't been correctly configured.");
  }

  AVStream* stream = 0;
  AVCodecParameters* codecParams = 0;
  if (_demuxCtx && _streamIdx >= 0 && _streamIdx < (int)_demuxCtx->nb_streams && _demuxCtx->streams[_streamIdx]) {
    stream = _demuxCtx->streams[_streamIdx];
    codecParams = _demuxCtx->streams[_streamIdx]->codecpar;
  }

  if (!_metadataSent) {
    int nChannels = guessChannelCount(_audioCtx, codecParams, _decodedFrame);
    Real sampleRate = guessSampleRate(_audioCtx, codecParams, _decodedFrame, stream);

    if (nChannels > 0 && sampleRate > 0) {
      writeChannelsSampleRateInfo(nChannels, sampleRate);
      _metadataChannels = nChannels;
      _metadataSampleRate = sampleRate;
      _metadataSent = true;
    }
  }

  if (!_codecInfoSent) {
    int bitRate = guessBitRate(_audioCtx, codecParams, _demuxCtx);
    std::string codec = guessCodecName(_audioCodec);

    writeCodecInfo(codec, bitRate);
    _metadataBitRate = bitRate;
    _metadataCodec = codec;
    _codecInfoSent = true;
  }

  do {
    int result = av_read_frame(_demuxCtx, &_packet);
    if (result != 0) {
      if (result != AVERROR_EOF) {
        char errstring[1204];
        av_strerror(result, errstring, sizeof(errstring));
        ostringstream msg;
        msg << "AudioLoader: Error reading frame: " << errstring;
        E_WARNING(msg.str());
      }

      shouldStop(true);
      flushPacket();

      AVStream* eofStream = 0;
      AVCodecParameters* eofCodecParams = 0;
      if (_demuxCtx && _streamIdx >= 0 && _streamIdx < (int)_demuxCtx->nb_streams && _demuxCtx->streams[_streamIdx]) {
        eofStream = _demuxCtx->streams[_streamIdx];
        eofCodecParams = _demuxCtx->streams[_streamIdx]->codecpar;
      }

      if (!_metadataSent) {
        int nChannels = guessChannelCount(_audioCtx, eofCodecParams, _decodedFrame);
        if (nChannels <= 0 && _nChannels > 0) {
          nChannels = _nChannels;
        }
        Real sampleRate = guessSampleRate(_audioCtx, eofCodecParams, _decodedFrame, eofStream);

        if (nChannels > 0 && sampleRate > 0) {
          writeChannelsSampleRateInfo(nChannels, sampleRate);
          _metadataChannels = nChannels;
          _metadataSampleRate = sampleRate;
          _metadataSent = true;
        }
      }

      if (!_codecInfoSent) {
        int bitRate = guessBitRate(_audioCtx, eofCodecParams, _demuxCtx);
        std::string codec = guessCodecName(_audioCodec);
        writeCodecInfo(codec, bitRate);
        _metadataBitRate = bitRate;
        _metadataCodec = codec;
        _codecInfoSent = true;
      }

      closeAudioFile();

      if (!_md5Sent) {
        if (_computeMD5) {
          av_md5_final(_md5Encoded, _checksum);
          writeMD5Info(uint8_t_to_hex(_checksum, 16));
        }
        else {
          writeMD5Info("");
        }
        _md5Sent = true;
      }

      return FINISHED;
    }
  } while (_packet.stream_index != _streamIdx);

  if (_computeMD5) {
    av_md5_update(_md5Encoded, _packet.data, _packet.size);
  }

  int consumed = decodePacket();
  (void)consumed;

  if (_dataSize > 0) {
    copyFFmpegOutput();
    _dataSize = 0;
  }

  av_packet_unref(&_packet);
  return OK;
}


int AudioLoader::decode_audio_frame(AVCodecContext* audioCtx,
                                    float* output,
                                    int* outputSize,
                                    AVPacket* packet) {
  int gotFrame = 0;
  av_frame_unref(_decodedFrame);

  int send_result = avcodec_send_packet(audioCtx, packet);
  if (send_result < 0) return send_result;

  int receive_result = avcodec_receive_frame(audioCtx, _decodedFrame);
  if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
    gotFrame = 0;
    return (packet->size > 0) ? packet->size : 0;
  }
  else if (receive_result < 0) {
    return receive_result;
  }
  gotFrame = 1;

  if (gotFrame) {
    if (_nChannels <= 0) {
      _nChannels = guessChannelCount(audioCtx, 0, _decodedFrame);
    }
    if (_nChannels <= 0) {
      throw EssentiaException("AudioLoader: Could not determine channel count from decoded frame");
    }

    int inputSamples = _decodedFrame->nb_samples;
    int inputPlaneSize = av_samples_get_buffer_size(NULL, _nChannels, inputSamples,
                                                    audioCtx->sample_fmt, 1);
    int outputPlaneSize = av_samples_get_buffer_size(NULL, _nChannels, inputSamples,
                                                     AV_SAMPLE_FMT_FLT, 1);
    int outputBufferSamples = *outputSize /
                              (av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT) * _nChannels);

    if (outputBufferSamples < inputSamples) {
      throw EssentiaException("AudioLoader: Insufficient buffer size for format conversion");
    }

    if (audioCtx->sample_fmt == AV_SAMPLE_FMT_FLT) {
      memcpy(output, _decodedFrame->data[0], inputPlaneSize);
    }
    else {
      int samplesWrittern = swr_convert(_convertCtxAv,
                                        (uint8_t**)&output,
                                        outputBufferSamples,
                                        (const uint8_t**)_decodedFrame->data,
                                        inputSamples);

      if (samplesWrittern < inputSamples) {
        ostringstream msg;
        msg << "AudioLoader: Incomplete format conversion (some samples missing)"
            << " from " << av_get_sample_fmt_name(_audioCtx->sample_fmt)
            << " to "   << av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT);
        throw EssentiaException(msg);
      }
    }
    *outputSize = outputPlaneSize;
  }
  else {
    E_DEBUG(EAlgorithm, "AudioLoader: tried to decode packet but didn't get any frame...");
    *outputSize = 0;
  }

  return packet->size;
}

void AudioLoader::flushPacket() {
  av_packet_unref(&_packet);
  AVPacket empty;
  av_init_packet(&empty);
  empty.data = NULL;
  empty.size = 0;

  while (true) {
    _dataSize = 0;
    int send_result = avcodec_send_packet(_audioCtx, &empty);
    if (send_result < 0 && send_result != AVERROR(EAGAIN)) {
      break;
    }
    int receive_result = avcodec_receive_frame(_audioCtx, _decodedFrame);
    if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
      break;
    }
    else if (receive_result < 0) {
      break;
    }

    if (_nChannels <= 0) {
      _nChannels = guessChannelCount(_audioCtx, 0, _decodedFrame);
    }
    if (_nChannels <= 0) {
      E_WARNING("AudioLoader: could not determine channel count while flushing decoder");
      break;
    }

    int inputSamples = _decodedFrame->nb_samples;
    int outPlaneSize = av_samples_get_buffer_size(NULL, _nChannels, inputSamples, AV_SAMPLE_FMT_FLT, 1);
    if (outPlaneSize > 0) {
      if (_audioCtx->sample_fmt == AV_SAMPLE_FMT_FLT) {
        memcpy(_buffer, _decodedFrame->data[0], (std::min)(outPlaneSize, FFMPEG_BUFFER_SIZE));
        _dataSize = (std::min)(outPlaneSize, FFMPEG_BUFFER_SIZE);
      }
      else {
        float* outBuff = (float*)_buffer;
        int samplesWritten = swr_convert(_convertCtxAv,
                                         (uint8_t**)&outBuff,
                                         inputSamples,
                                         (const uint8_t**)_decodedFrame->data,
                                         inputSamples);
        if (samplesWritten > 0) {
          _dataSize = (std::min)(samplesWritten * _nChannels * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT),
                                 FFMPEG_BUFFER_SIZE);
        }
      }
    }

    if (_dataSize > 0) {
      copyFFmpegOutput();
      _dataSize = 0;
    }
  }
}


/**
 * Gets the AVPacket stored in _packet, and decodes all the samples it can from it,
 * putting them in _buffer, the total number of bytes written being stored in _dataSize.
 */
int AudioLoader::decodePacket() {
  float* outBuff = (float*)_buffer;
  _dataSize = 0;

  int send_result = avcodec_send_packet(_audioCtx, &_packet);
  if (send_result == AVERROR(EAGAIN)) {
    // decoder not ready to accept packet; try receiving frames first
  }
  else if (send_result < 0) {
    char errstring[1204];
    av_strerror(send_result, errstring, sizeof(errstring));
    E_WARNING("AudioLoader: avcodec_send_packet error: " << errstring);
    return 0;
  }

  int receive_result = avcodec_receive_frame(_audioCtx, _decodedFrame);
  if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
    return 0;
  }
  else if (receive_result < 0) {
    char errstring[1204];
    av_strerror(receive_result, errstring, sizeof(errstring));
    E_WARNING("AudioLoader: avcodec_receive_frame error: " << errstring);
    return 0;
  }

  if (_nChannels <= 0) {
    _nChannels = guessChannelCount(_audioCtx, 0, _decodedFrame);
  }
  if (_nChannels <= 0) {
    E_WARNING("AudioLoader: could not determine channel count from decoded frame");
    return 0;
  }

  int inputSamples = _decodedFrame->nb_samples;
  int outPlaneSize = av_samples_get_buffer_size(NULL, _nChannels, inputSamples, AV_SAMPLE_FMT_FLT, 1);
  if (outPlaneSize <= 0) {
    E_WARNING("AudioLoader: computed non-positive outPlaneSize");
    return 0;
  }

  if (outPlaneSize > FFMPEG_BUFFER_SIZE) {
    ostringstream msg;
    msg << "AudioLoader: required buffer " << outPlaneSize << " exceeds allocated " << FFMPEG_BUFFER_SIZE;
    E_WARNING(msg.str());
  }

  if (_audioCtx->sample_fmt == AV_SAMPLE_FMT_FLT) {
    memcpy(outBuff, _decodedFrame->data[0], (std::min)(outPlaneSize, FFMPEG_BUFFER_SIZE));
  }
  else {
    int samplesWritten = swr_convert(_convertCtxAv,
                                     (uint8_t**)&outBuff,
                                     inputSamples,
                                     (const uint8_t**)_decodedFrame->data,
                                     inputSamples);
    if (samplesWritten <= 0) {
      E_WARNING("AudioLoader: swr_convert returned no samples");
      return 0;
    }
    outPlaneSize = samplesWritten * _nChannels * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
  }

  _dataSize = (std::min)(outPlaneSize, FFMPEG_BUFFER_SIZE);
  return _packet.size;
}


void AudioLoader::copyFFmpegOutput() {
  int bytesPerSample = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
  int nsamples = _dataSize / (bytesPerSample * _nChannels);
  if (nsamples == 0) return;

  bool ok = _audio.acquire(nsamples);
  if (!ok) {
    throw EssentiaException("AudioLoader: could not acquire output for audio");
  }

  vector<StereoSample>& audio = *((vector<StereoSample>*)_audio.getTokens());
  float* fbuf = (float*)_buffer;

  if (_nChannels == 1) {
    for (int i = 0; i < nsamples; i++) {
      audio[i].left() = fbuf[i];
    }
  }
  else {
    for (int i = 0; i < nsamples; i++) {
      audio[i].left() = fbuf[2*i];
      audio[i].right() = fbuf[2*i+1];
    }
  }

  _audio.release(nsamples);
}


void AudioLoader::reset() {
  Algorithm::reset();

  if (!parameter("filename").isConfigured()) return;

  string filename = parameter("filename").toString();

  closeAudioFile();
  openAudioFile(filename);

  _metadataSent = false;
  _codecInfoSent = false;
  _md5Sent = false;
  _metadataChannels = 0;
  _metadataSampleRate = 0.0;
  _metadataBitRate = 0;
  _metadataCodec = (_audioCodec && _audioCodec->name) ? _audioCodec->name : "";

  AVCodecParameters* codecParams = 0;
  if (_demuxCtx && _streamIdx >= 0 && _streamIdx < (int)_demuxCtx->nb_streams && _demuxCtx->streams[_streamIdx]) {
    codecParams = _demuxCtx->streams[_streamIdx]->codecpar;
  }
  AVStream* stream = (_demuxCtx && _streamIdx >= 0 && _streamIdx < (int)_demuxCtx->nb_streams)
                   ? _demuxCtx->streams[_streamIdx]
                   : 0;

  _metadataChannels = guessChannelCount(_audioCtx, codecParams, 0);
  _metadataSampleRate = guessSampleRate(_audioCtx, codecParams, 0, stream);
  _metadataBitRate = guessBitRate(_audioCtx, codecParams, _demuxCtx);

  _nChannels = _metadataChannels;
  if (_nChannels <= 0 && _audioCtx) {
    _nChannels = guessChannelCount(_audioCtx, 0, 0);
  }
}

} // namespace streaming
} // namespace essentia


namespace essentia {
namespace standard {

const char* AudioLoader::name = "AudioLoader";
const char* AudioLoader::category = "Input/output";
const char* AudioLoader::description = DOC("This algorithm loads the single audio stream contained in a given audio or video file. Supported formats are all those supported by the FFmpeg library including wav, aiff, flac, ogg and mp3.\n"
"\n"
"This algorithm will throw an exception if it was not properly configured which is normally due to not specifying a valid filename. Invalid names comprise those with extensions different than the supported  formats and non existent files. If using this algorithm on Windows, you must ensure that the filename is encoded as UTF-8\n\n"
"Note: ogg files are decoded in reverse phase, due to be using ffmpeg library.\n"
"\n"
"References:\n"
"  [1] WAV - Wikipedia, the free encyclopedia,\n"
"      http://en.wikipedia.org/wiki/Wav\n"
"  [2] Audio Interchange File Format - Wikipedia, the free encyclopedia,\n"
"      http://en.wikipedia.org/wiki/Aiff\n"
"  [3] Free Lossless Audio Codec - Wikipedia, the free encyclopedia,\n"
"      http://en.wikipedia.org/wiki/Flac\n"
"  [4] Vorbis - Wikipedia, the free encyclopedia,\n"
"      http://en.wikipedia.org/wiki/Vorbis\n"
"  [5] MP3 - Wikipedia, the free encyclopedia,\n"
"      http://en.wikipedia.org/wiki/Mp3");


void AudioLoader::createInnerNetwork() {
  _loader = streaming::AlgorithmFactory::create("AudioLoader");
  _audioStorage = new streaming::VectorOutput<StereoSample>();

  _loader->output("audio")           >>  _audioStorage->input("data");
  _loader->output("sampleRate")      >>  PC(_pool, "internal.sampleRate");
  _loader->output("numberChannels")  >>  PC(_pool, "internal.numberChannels");
  _loader->output("md5")             >>  PC(_pool, "internal.md5");
  _loader->output("codec")           >>  PC(_pool, "internal.codec");
  _loader->output("bit_rate")        >>  PC(_pool, "internal.bit_rate");
  _network = new scheduler::Network(_loader);
}

void AudioLoader::configure() {
  _loader->configure(INHERIT("filename"),
                     INHERIT("computeMD5"),
                     INHERIT("audioStream"));
}

void AudioLoader::compute() {
  if (!parameter("filename").isConfigured()) {
    throw EssentiaException("AudioLoader: Trying to call compute() on an "
                            "AudioLoader algo which hasn't been correctly configured.");
  }

  Real& sampleRate = _sampleRate.get();
  int& numberChannels = _channels.get();
  string& md5 = _md5.get();
  int& bit_rate = _bit_rate.get();
  string& codec = _codec.get();
  vector<StereoSample>& audio = _audio.get();

  _audioStorage->setVector(&audio);

  _network->run();

  sampleRate = _pool.value<Real>("internal.sampleRate");
  numberChannels = (int)_pool.value<Real>("internal.numberChannels");
  md5 = _pool.value<std::string>("internal.md5");
  bit_rate = (int)_pool.value<Real>("internal.bit_rate");
  codec = _pool.value<std::string>("internal.codec");

  reset();
}

void AudioLoader::reset() {
  _network->reset();
  _pool.remove("internal.md5");
  _pool.remove("internal.sampleRate");
  _pool.remove("internal.numberChannels");
  _pool.remove("internal.codec");
  _pool.remove("internal.bit_rate");
}

} // namespace standard
} // namespace essentia
