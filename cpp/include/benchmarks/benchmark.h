/*
 * Copyright (c) 2014 by Johannes Dillmann, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <cstddef>
#include <string>

#include "pbrpc/RPC.pb.h" // UserCredentials
#include "benchmarks/benchmark_options.h"
#include "libxtreemfs/client.h"
#include "libxtreemfs/file_handle.h"
#include "libxtreemfs/volume.h"
#include "rpc/ssl_options.h" // SSLOptions

using namespace std;
using namespace xtreemfs::pbrpc;

namespace xtreemfs {

class BenchmarkResult {
 public:
  BenchmarkResult(string type, int run, double time_us, size_t requested_size,
                  size_t processed_size);

  BenchmarkResult(string type, int run, double time_us, size_t requested_size,
                  size_t processed_size, int parallel);

  /** Get the header of the used CSV format. */
  static string csv_header();

  /** Get the benchmark results as a CSV row. */
  string as_csv_row();

  /** Get the type of the benchmark. */
  const string& type() const;

  /** Get the time in microseconds the benchmark run took. */
  double time_us() const;

  /** Get the size in bytes the benchmark was requested to write or read. */
  size_t requested_size() const;

  /** Get the size in bytes that has been written or read by the benchmark. */
  size_t processed_size() const;

  /** Get the number of parallel benchmark threads. */
  int parallel() const;

  /** Set the number of parallel benchmark threads. */
  void set_parallel(int parallel);

  /** Get the number of the run this benchmark belonged to. */
  int run() const;

 private:
  /** The type of the benchmark. */
  string type_;

  /** The number of the run this benchmark belonged to. */
  int run_;

  /** Time in microseconds the benchmark run took. */
  double time_us_;

  /** Size in bytes the benchmark was requested to write or read. */
  size_t requested_size_;

  /** Size in bytes that has been written or read by the benchmark. */
  size_t processed_size_;

  /** The number of parallel benchmark threads. */
  int parallel_;
};

class Benchmark {
 public:

  /** Create a Benchmark. */
  Benchmark(pbrpc::UserCredentials& user_credentials,
            BenchmarkOptions& options);

  ~Benchmark();

  /** Set the client used by this benchmark instance. */
  void init(Client* client);

  /** Delete files and volumes, that have been created by this benchmark. */
  void cleanup();

  /** Open the specified XtreemFS Volume and ensure it is suitable for benchmarking. */
  void prepareVolume(std::string& volume_name);

  /** Create the default benchmarking volume # number.
   *  Volumes created this way will be deleted upon destruction.
   */
  string createAndPrepareVolume(int number);

  /** Perform a single sequential write benchmark. */
  BenchmarkResult performSequentialWrite(char* data, size_t size,
                                         long bench_size, int run);

 private:
  /** Open and prepare the volume specified in volume_name_. */
  void prepareVolume();

  /** Deletes the volume if it has been created by this benchmark. */
  void clearVolume();

  /** Ensure the file used for the benchmark exists but is truncated. */
  void prepareFile(string type, int run);

  /** Delete the file used by the previous benchmark run, if it exists. */
  void clearFile();

  /** Clear the benchmark directory on the volume
   *  and returns if the the directory does exist.
   *
   * @param delete_dir  Delete the benchmark directory if it exists.
   */
  bool clearDirectory(bool delete_dir);

  /** Name and Groups of the user. */
  pbrpc::UserCredentials user_credentials_;

  /** SSL options, if set. */
  rpc::SSLOptions* ssl_options_;

  /** Authentication used for operation. */
  Auth auth_;

  /** Benchmark options. */
  BenchmarkOptions options_;

  /** Client to use */
  Client* client_;

  /** Volume use for benchmarks. */
  Volume* volume_;

  /** Name of the used Volume. */
  string volume_name_;

  /** Flag indicating if the volume has been created by this benchmark. */
  bool volume_created_;

  /** File path of the last file used for benchmarks. */
  string file_path_;

  /** Directory path used for benchmarks. */
  static const string dir_path_;

  /** Basename used for creating benchmark volumes. */
  static const string volume_basename_;
};

}  // namespace xtreemfs

#endif /* BENCHMARK_H_ */
