/*
 * Copyright (c) 2014 by Johannes Dillmann, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#ifndef BENCHMARK_OPTIONS_H_
#define BENCHMARK_OPTIONS_H_

#include "libxtreemfs/options.h"

#include <boost/program_options.hpp>
#include <string>

namespace xtreemfs {

class BenchmarkOptions : public Options {
 public:
  /** Sets the default values. */
  BenchmarkOptions();

  /** Set options parsed from command line which must contain at least the URL
   *  to a XtreemFS volume.
   *
   *  Calls Options::ParseCommandLine() to parse general options.
   *
   * @throws InvalidCommandLineParametersException
   * @throws InvalidURLException */
  void ParseCommandLine(int argc, char** argv);

  /** Shows only the minimal help text describing the usage of rmfs.xtreemfs.*/
  std::string ShowCommandLineUsage();

  /** Outputs usage of the command line parameters. */
  virtual std::string ShowCommandLineHelp();

  /** Number of parallel benchmarks.*/
  int num;

  /** Number of times to run each benchmark case. */
  int runs;

  /** Maximum number of OSDs a file is distributed to. */
  int stripe_width;

  /** Size of an OSD storage block ("blocksize") in bytes. */
  long stripe_size;

  /**
   * Size for reads/writes in benchmarks.
   * The chuck size is the amount of data written/read in one piece.
   */
  long chunk_size;

  /** Size in bytes used for sequential benchmarks. */
  long sequential_size;

  /** Run sequential write benchmarks if this is set. */
  bool run_sw;

  /** Administrator password to authorize operations. */
  std::string admin_password;

  /** Address to the used DIR */
  ServiceAddresses dir_address;

  /** Address to the used MRC */
  ServiceAddresses mrc_address;

  /** Names of the Volumes used for benchmarks. */
  std::vector<std::string> volume_names;

  /** Flag to indicate if volumes have to be created. */
  bool create_volumes;

 private:
  /** Contains all available benchmark options and its descriptions. */
  boost::program_options::options_description benchmark_descriptions_;

  /** Brief help text if there are no command line arguments. */
  std::string helptext_usage_;
};

}  // namespace xtreemfs

#endif /* BENCHMARK_OPTIONS_H_ */
