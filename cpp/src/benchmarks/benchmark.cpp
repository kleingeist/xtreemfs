/*
 * Copyright (c) 2014 by Johannes Dillmann, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#include "benchmarks/benchmark.h"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <sstream>
#include <string>

#include "benchmarks/benchmark_options.h"
#include "benchmarks/clock.hpp"
#include "json/json.h"
#include "libxtreemfs/system_user_mapping.h"
#include "libxtreemfs/user_mapping.h"
#include "libxtreemfs/xtreemfs_exception.h"

using namespace std;
using namespace xtreemfs;
using namespace xtreemfs::pbrpc;

int main(int argc, char* argv[]) {
  // Parse command line options.
  BenchmarkOptions options;
  bool invalid_commandline_parameters = false;
  try {
    options.ParseCommandLine(argc, argv);
  } catch (const XtreemFSException& e) {
    cout << "Invalid parameters found, error: " << e.what() << endl << endl;
    invalid_commandline_parameters = true;
  }
  // Display help if needed.
  if (options.empty_arguments_list || invalid_commandline_parameters) {
    cout << options.ShowCommandLineUsage() << endl;
    return 1;
  }
  if (options.show_help) {
    cout << options.ShowCommandLineHelp() << endl;
    return 1;
  }
  // Show only the version.
  if (options.show_version) {
    cout << options.ShowVersion("benchmark") << endl;
    return 1;
  }

  // Set user_credentials.
  boost::scoped_ptr<SystemUserMapping> system_user_mapping;
  system_user_mapping.reset(SystemUserMapping::GetSystemUserMapping());
  // Check if the user specified an additional user mapping in options.
  UserMapping* additional_um = UserMapping::CreateUserMapping(
      options.additional_user_mapping_type, options);
  if (additional_um) {
    system_user_mapping->RegisterAdditionalUserMapping(additional_um);
    system_user_mapping->StartAdditionalUserMapping();
  }

  UserCredentials user_credentials;
  system_user_mapping->GetUserCredentialsForCurrentUser(&user_credentials);
  if (user_credentials.username().empty()) {
    cout << "Error: No name found for the current user (using the configured "
         "UserMapping: "
         << options.additional_user_mapping_type << ")\n";
    return 1;
  }
  // The groups won't be checked and therefore may be empty.

  // Initialize test data
  long block_size = options.chunk_size;
  char* block_data = new char[block_size];
  long bench_size = options.sequential_size;

  // Initialize volumes
  vector<Benchmark::SharedPtr> benchmarks;
  for (int i = 0; i < options.num; ++i) {
    Benchmark::SharedPtr benchmark(
        new Benchmark(user_credentials, options));
    benchmarks.push_back(benchmark);

    benchmark->init();

    if (options.create_volumes) {
      benchmark->createAndPrepareVolume(i);
    } else {
      benchmark->prepareVolume(options.volume_names[i]);
    }
  }

  vector<BenchmarkResult> results;

  // Run the benchmarks
  for (int i = 0; i < options.runs; ++i) {

    // unique_future is non copyable and non movable, so we have to wrap it in a shared_future
    // TODO: c++11 futures can be moved
    vector<boost::shared_future<BenchmarkResult> > future_results;

    // Run each benchmark in parallel.
    for (vector<Benchmark::SharedPtr>::iterator it = benchmarks.begin();
        it != benchmarks.end(); ++it) {
      Benchmark::SharedPtr benchmark = *it;

      if (options.run_sw) {
        // Prepare thread
        boost::packaged_task<BenchmarkResult> pt(
            boost::bind(&Benchmark::performSequentialWrite, benchmark,
                        block_data, block_size, bench_size, i));
        boost::shared_future<BenchmarkResult> f(pt.get_future());
        future_results.push_back(f);

        // Run benchmark in another thread
        boost::thread task(boost::move(pt));
      }
    }

    // Wait for results
    boost::wait_for_all(future_results.begin(), future_results.end());

    // Gather results
    for (vector<boost::shared_future<BenchmarkResult> >::iterator it =
        future_results.begin(); it != future_results.end(); ++it) {
      // Complete the result object and store it.
      BenchmarkResult r = it->get();
      r.set_parallel(options.num);
      results.push_back(r);
    }

    future_results.clear();
  }

  // Output results
  cout << BenchmarkResult::csv_header() << endl;
  for (vector<BenchmarkResult>::iterator it = results.begin();
      it != results.end(); ++it) {
    cout << it->as_csv_row() << endl;
  }

  // Cleanup
  return 0;
}

namespace xtreemfs {

const string Benchmark::dir_path_ = "benchmarks";
const string Benchmark::volume_basename_ = "benchmark";

Benchmark::Benchmark(pbrpc::UserCredentials& user_credentials, BenchmarkOptions& options)
    : user_credentials_(user_credentials),
      options_(options) {

  ssl_options_ = options.GenerateSSLOptions();

  if (options.admin_password.empty()) {
    auth_.set_auth_type(AUTH_NONE);
  } else {
    auth_.set_auth_type(AUTH_PASSWORD);
    auth_.mutable_auth_passwd()->set_password(options.admin_password);
  }

  volume_ = NULL;
  volume_created_ = false;
  client_ = NULL;
}

Benchmark::~Benchmark() {
  if (volume_ != NULL) {
    clearDirectory(true);
    volume_->Close();
  }

  if (client_ != NULL) {
    clearVolume();
    client_->Shutdown();
    delete client_;
  }

  delete ssl_options_;
}

void Benchmark::init() {
  client_ = Client::CreateClient(options_.dir_address, user_credentials_,
                                 ssl_options_, options_);
  client_->Start();
}

bool Benchmark::clearDirectory(bool delete_dir) {
  if (volume_ == NULL) {
    throw new XtreemFSException(
        "Volume has to be opened before calling clearVolume.");
  }

  bool dir_exists = true;

  boost::scoped_ptr<DirectoryEntries> entries;
  int max_entries = 100;

  for (int o = 0; true; o = o + max_entries) {
    // Try to read the directory entries.
    try {
      entries.reset(
          volume_->ReadDir(user_credentials_, dir_path_, o, max_entries, true));
    } catch (const PosixErrorException& e) {
      if (e.posix_errno() == POSIX_ERROR_ENOENT) {
        dir_exists = false;
        break;
      }
      throw;
    }

    if (entries->entries_size() == 0) {
      break;
    }

    // Delete the directory entries.
    for (int i = 0; i < entries->entries_size(); ++i) {
      string entry = entries->entries(i).name();

      if (entry == "." || entry == "..") {
        continue;
      }

      ostringstream ss;
      ss << dir_path_ << "/" << entry;

      try {
        volume_->Unlink(user_credentials_, ss.str());
      } catch (const PosixErrorException& e) {
        if (e.posix_errno() == POSIX_ERROR_ENOENT) {
          continue;
        }
        throw;
      }

    }
  }

  if (dir_exists && delete_dir) {
    // TODO: delete parent dirs
    volume_->DeleteDirectory(user_credentials_, dir_path_);
    dir_exists = false;
  }

  return dir_exists;
}

string Benchmark::createAndPrepareVolume(int number) {
  volume_created_ = true;

  ostringstream ss;
  ss << volume_basename_ << number;
  volume_name_ = ss.str();

  Auth auth;
  auth.set_auth_type(AUTH_NONE);
  client_->CreateVolume(options_.mrc_address, auth, user_credentials_,
                        volume_name_);

  prepareVolume();

  return volume_name_;
}

void Benchmark::clearVolume() {
  if (!volume_created_) {
    return;
  }

  Auth auth;
  auth.set_auth_type(AUTH_NONE);
  client_->DeleteVolume(options_.mrc_address, auth, user_credentials_, volume_name_);
}

void Benchmark::prepareVolume(std::string& volume_name) {
  volume_created_ = false;
  volume_name_ = volume_name;
  prepareVolume();
}

void Benchmark::prepareVolume() {
  volume_ = client_->OpenVolume(volume_name_, ssl_options_, options_);

  // Create or clear the benchmark directory.
  bool dir_exists = clearDirectory(false);
  if (!dir_exists) {
    // TODO: make parent dirs
    volume_->MakeDirectory(user_credentials_, dir_path_, 0777);
  }

  // Set the requested striping policy.
  Json::Value xattr_value(Json::objectValue);
  xattr_value["pattern"] = "STRIPING_POLICY_RAID0";
  xattr_value["width"] = options_.stripe_width;

  int stripe_size_KiB = (int) (options_.stripe_size / 1024);
  xattr_value["size"] = stripe_size_KiB;

  Json::FastWriter writer;
  volume_->SetXAttr(user_credentials_, dir_path_, "xtreemfs.default_sp",
                    writer.write(xattr_value),
                    xtreemfs::pbrpc::XATTR_FLAGS_REPLACE);
}

void Benchmark::prepareFile(string type, int run) {
  // Clear leftovers.
  clearFile();

  // Generate the path of the current benchmark.
  ostringstream ss;
  ss << dir_path_ << "/" << type << "-" << run;
  file_path_ = ss.str();

  // Ensure the file exists and is truncated.
  FileHandle* file =
      volume_->OpenFile(
          user_credentials_,
          file_path_,
          static_cast<xtreemfs::pbrpc::SYSTEM_V_FCNTL>(xtreemfs::pbrpc::SYSTEM_V_FCNTL_H_O_CREAT
              | xtreemfs::pbrpc::SYSTEM_V_FCNTL_H_O_TRUNC
              | xtreemfs::pbrpc::SYSTEM_V_FCNTL_H_O_WRONLY),
          511);
  file->Close();
}

void Benchmark::clearFile() {
  if (file_path_.length() > 0) {
    try {
      volume_->Unlink(user_credentials_, file_path_);
    } catch (const PosixErrorException& e) {
      if (e.posix_errno() != POSIX_ERROR_ENOENT) {
        throw;
      }
    }

    file_path_.clear();
  }
}

BenchmarkResult Benchmark::performSequentialWrite(char* data, size_t size,
                                                  long bench_size, int run) {

  string type = "SEQ_WRITE";

  prepareFile(type, run);

  // Ensure the file sizes are correct.
  // TODO: has to be called only once for every Benchmark
  //  if (bench_size % size != 0) {
  //    throw new XtreemFSException("size must satisfy: size % (stripeSize * stripeWidth) == 0"");
  //  }

  long blocks = bench_size / size;
  size_t size_effective = blocks * size;

  FileHandle* file;

  // Start clock.
  cb::time::WallClock clock;

  // Open the file
  file =
      volume_->OpenFile(
          user_credentials_,
          file_path_,
          static_cast<xtreemfs::pbrpc::SYSTEM_V_FCNTL>(xtreemfs::pbrpc::SYSTEM_V_FCNTL_H_O_CREAT
              | xtreemfs::pbrpc::SYSTEM_V_FCNTL_H_O_WRONLY),
          511);

  // Perform IO.
  for (long i = 0; i < blocks; ++i) {
    file->Write(data, size, size * i);
  }

  // Flushing and closing is required to wait for async calls.
  file->Flush();
  file->Close();

  // End Clock.
  double elapsed = clock.elapsed();

  // Cleanup.
  clearFile();

  BenchmarkResult result(type, run, elapsed, size_effective, size_effective);
  return result;
}

BenchmarkResult::BenchmarkResult(string type, int run, double time_us,
                                 size_t requested_size, size_t processed_size)
    : type_(type),
      run_(run),
      time_us_(time_us),
      requested_size_(requested_size),
      processed_size_(processed_size),
      parallel_(0) {
}

BenchmarkResult::BenchmarkResult(string type, int run, double time_us,
                                 size_t requested_size, size_t processed_size,
                                 int parallel)
    : type_(type),
      run_(run),
      time_us_(time_us),
      requested_size_(requested_size),
      processed_size_(processed_size),
      parallel_(parallel) {
}

string BenchmarkResult::csv_header() {
  return "Type;NumberOfParallelThreads;TimeInSec;MiB/Sec;DataWrittenInBytes;ByteCount;Run";
}

string BenchmarkResult::as_csv_row() {
  double time_s = time_us_ / 1000000;
  double throughput_MiBs = (processed_size_ / (1024 * 1024)) / time_s;

  ostringstream ss;
  ss << type_ << ";" << parallel_ << ";" << time_s << ";" << throughput_MiBs
     << ";" << processed_size_ << ";" << requested_size_ << ";" << run_;

  string row = ss.str();
  return row;
}

int BenchmarkResult::parallel() const {
  return parallel_;
}

void BenchmarkResult::set_parallel(int parallel) {
  parallel_ = parallel;
}

size_t BenchmarkResult::processed_size() const {
  return processed_size_;
}

size_t BenchmarkResult::requested_size() const {
  return requested_size_;
}

int BenchmarkResult::run() const {
  return run_;
}

double BenchmarkResult::time_us() const {
  return time_us_;
}

const string& BenchmarkResult::type() const {
  return type_;
}

}
