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

#ifndef ESSENTIA_POOLSTORAGE_H
#define ESSENTIA_POOLSTORAGE_H

#include "../streamingalgorithm.h"
#include "../../pool.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace essentia {
namespace streaming {

class PoolStorageBase : public Algorithm {
 protected:
  Pool* _pool;
  std::string _descriptorName;
  bool _setSingle;

 public:
  PoolStorageBase(Pool* pool, const std::string& descriptorName, bool setSingle = false) :
    _pool(pool), _descriptorName(descriptorName), _setSingle(setSingle) {}

  ~PoolStorageBase() {}

  const std::string& descriptorName() const {
    return _descriptorName;
  }

  Pool* pool() const {
    return _pool;
  }

};

inline void poolStorageDebugPrintValue(const std::string& descriptorName, const Real& value) {
  fprintf(stderr, "PoolStorage write desc='%s' scalar Real=%f\n",
          descriptorName.c_str(), (double)value);
  fflush(stderr);
}

inline void poolStorageDebugPrintValue(const std::string& descriptorName, const int& value) {
  fprintf(stderr, "PoolStorage write desc='%s' scalar int=%d\n",
          descriptorName.c_str(), value);
  fflush(stderr);
}

inline void poolStorageDebugPrintValue(const std::string& descriptorName, const std::string& value) {
  fprintf(stderr, "PoolStorage write desc='%s' scalar string='%s'\n",
          descriptorName.c_str(), value.c_str());
  fflush(stderr);
}

template <typename T>
inline void poolStorageDebugPrintValue(const std::string& descriptorName, const T&) {
  fprintf(stderr, "PoolStorage write desc='%s' scalar <unprintable-type>\n",
          descriptorName.c_str());
  fflush(stderr);
}

template <typename T>
inline void poolStorageDebugPrintVectorHead(const std::string& descriptorName, const std::vector<T>& values) {
  fprintf(stderr, "PoolStorage append desc='%s' count=%d\n",
          descriptorName.c_str(), (int)values.size());
  fflush(stderr);
}

template <typename TokenType, typename StorageType = TokenType>
class PoolStorage : public PoolStorageBase {
 protected:
  Sink<TokenType> _descriptor;

 public:
  PoolStorage(Pool* pool, const std::string& descriptorName, bool setSingle = false) :
    PoolStorageBase(pool, descriptorName, setSingle) {

    setName("PoolStorage");
    declareInput(_descriptor, 1, "data", "the input data");
  }

  ~PoolStorage() {}

  void declareParameters() {}

  AlgorithmStatus process() {
    EXEC_DEBUG("process(), for desc: " << _descriptorName);

    fprintf(stderr, "PoolStorage process desc='%s' available=%d maxContiguous=%d setSingle=%d\n",
            _descriptorName.c_str(),
            _descriptor.available(),
            _descriptor.buffer().bufferInfo().maxContiguousElements,
            (int)_setSingle);
    fflush(stderr);

    int ntokens = (std::min)(_descriptor.available(),
                           _descriptor.buffer().bufferInfo().maxContiguousElements);
    ntokens = (std::max)(ntokens, 1);

    // for singleFrames buffer usage, the size of the phantom zone may be zero,
    // thus need to  +1. And we're still on the safe side, see acquireForRead (phantombuffer_impl.cpp)
    EXEC_DEBUG("trying to acquire " << ntokens << " tokens");
    if (!_descriptor.acquire(ntokens)) {
      fprintf(stderr, "PoolStorage acquire failed desc='%s' ntokens=%d\n",
              _descriptorName.c_str(), ntokens);
      fflush(stderr);
      return NO_INPUT;
    }

    EXEC_DEBUG("appending tokens to pool");
    if (ntokens > 1) {
      poolStorageDebugPrintVectorHead(_descriptorName, _descriptor.tokens());
      _pool->append(_descriptorName, _descriptor.tokens());
    }
    else {
      StorageType value = (StorageType)_descriptor.firstToken();
      poolStorageDebugPrintValue(_descriptorName, value);
      addToPool(value);
    }

    EXEC_DEBUG("releasing");
    fprintf(stderr, "PoolStorage release desc='%s' ntokens=%d\n",
            _descriptorName.c_str(), ntokens);
    fflush(stderr);
    _descriptor.release(ntokens);

    return OK;
  }

  template <typename T>
  void addToPool(const std::vector<T>& value) {
    if (_setSingle) {
      for (int i=0; i<(int)value.size();++i)
      _pool->add(_descriptorName, value[i]);
    }
    else _pool->add(_descriptorName, value);
  }

  void addToPool(const std::vector<Real>& value) {
    if (_setSingle) _pool->set(_descriptorName, value);
    else            _pool->add(_descriptorName, value);
  }

  template <typename T>
  void addToPool(const T& value) {
    if (_setSingle) _pool->set(_descriptorName, value);
    else            _pool->add(_descriptorName, value);
   }

  template <typename T>
  void addToPool(const TNT::Array2D<T>& value) {
    _pool->add(_descriptorName, value);
    /*
      if (_setSingle) {
      throw EssentiaException("PoolStorage::addToPool, setting Array2D as single value"
                              " is not supported by Pool.");
      }
      else _pool->add(_descriptorName, value);
    */
  }

  template <typename T>
  void addToPool(const Tensor<T>& value) {
    _pool->add(_descriptorName, value);
  }

  void addToPool(const StereoSample& value) {
    if (_setSingle) {
      throw EssentiaException("PoolStorage::addToPool, setting StereoSample as single value"
                              " is not supported by Pool.");
    }
    else {
      _pool->add(_descriptorName, value);
    }
  }

};


/**
 * Connect a source (eg: the output of an algorithm) to a Pool, and use the given
 * name as an identifier in the Pool.
 */
void connect(SourceBase& source, Pool& pool,
             const std::string& descriptorName);

class PoolConnector {
 protected:
  Pool& pool;
  std::string name;

 public:
  PoolConnector(Pool& p, const std::string& descName) : pool(p), name(descName) {}

  friend void operator>>(SourceBase& source, const PoolConnector& pc);
};

#define PC essentia::streaming::PoolConnector

// The reason why this function is defined with a const PC& as argument is described here:
// http://herbsutter.com/2008/01/01/gotw-88-a-candidate-for-the-most-important-const/
inline void operator>>(SourceBase& source, const PoolConnector& pc) {
  connect(source, pc.pool, pc.name);
}

/**
 * Connect a source (eg: the output of an algorithm) to a Pool, and use the given
 * name as an identifier in the Pool. Forces the use of the Pool::set method,
 * instead of Pool::add.
 */
void connectSingleValue(SourceBase& source, Pool& pool,
                        const std::string& descriptorName);

/**
 * Disconnect a source (eg: the output of an algorithm) from a Pool.
 */
void disconnect(SourceBase& source, Pool& pool,
                const std::string& descriptorName);


} // namespace streaming
} // namespace essentia

#endif // ESSENTIA_POOLSTORAGE_H
