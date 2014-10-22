/*
 * Copyright (c) 2014 by Johannes Dillmann, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <cstddef>
#include <boost/shared_ptr.hpp>
#include <string>

#include "pbrpc/RPC.pb.h" // UserCredentials
#include "benchmarks/benchmark_options.h"
#include "libxtreemfs/client.h"
#include "libxtreemfs/file_handle.h"
#include "libxtreemfs/volume.h"
#include "rpc/ssl_options.h" // SSLOptions

using namespace std;

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

  /** Time in microseconds the benchmark run took. */
  double time_us_;

  /** Size in bytes the benchmark was requested to write or read. */
  size_t requested_size_;

  /** Size in bytes that has been written or read by the benchmark. */
  size_t processed_size_;

  /** The number of parallel benchmark threads. */
  int parallel_;

  /** The number of the run this benchmark belonged to. */
  int run_;
};

class Benchmark {
 public:


  /**
   * Create a Benchmark.
   *
   * @remark Ownership of ssl_options IS transferred to the benchmark.
   */
  Benchmark(ServiceAddresses& dir_service_addresses,
            pbrpc::UserCredentials& user_credentials,
            rpc::SSLOptions* ssl_options, BenchmarkOptions& options);

  ~Benchmark();

  /** Start a XtreemFS Client used by exclusively by this benchmark instance. */
  void init();

  /** Open the specified XtreemFS Volume and ensure it is suitable for benchmarking. */
  void prepareVolume(std::string& volume_name);

  /** Perform a single sequential write benchmark. */
  BenchmarkResult performSequentialWrite(char* data, size_t size,
                                           long bench_size, int run);

  typedef boost::shared_ptr<Benchmark> SharedPtr;

 private:
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

  /**  List of DIR replicas */
  ServiceAddresses dir_service_addresses_;

  /** Name and Groups of the user. */
  pbrpc::UserCredentials user_credentials_;

  /** SSL options, if set. */
  rpc::SSLOptions* ssl_options_;

  /** Benchmark options. */
  BenchmarkOptions options_;

  /** Client used for benchmarks. */
  Client* client_;

  /** Volume use for benchmarks. */
  Volume* volume_;

  /** File path of the last file used for benchmarks. */
  string file_path_;

  /** Directory path used for benchmarks. */
  static const string dir_path_;
};

}  // namespace xtreemfs

#endif /* BENCHMARK_H_ */
