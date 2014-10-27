/*
 * Copyright (c) 2014 by Johannes Dillmann, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#include "benchmarks/benchmark_options.h"

#include <boost/program_options/cmdline.hpp>
#include <iostream>
#include <sstream>

#include "libxtreemfs/helper.h"
#include "libxtreemfs/pbrpc_url.h"
#include "libxtreemfs/xtreemfs_exception.h"

using namespace std;
using namespace xtreemfs::pbrpc;

namespace po = boost::program_options;
namespace style = boost::program_options::command_line_style;

namespace xtreemfs {

BenchmarkOptions::BenchmarkOptions()
    : Options(),
      benchmark_descriptions_("Benchmark Options") {

  helptext_usage_ =
      "benchmark: Run benchmarks on specified Volumes.\n"
      "\n"
      "Usage:\n"
      "\tbenchmark [options] volume1 volume2 ...\n"
      "\n"
      "The number of volumes must be in accordance with the number of benchmarks to be\n"
      "started in parallel. If no volume is given, n volumes will be created for the test.\n"
      "\n";

  benchmark_descriptions_.add_options()
      ("number,n", po::value<int>(&num)->default_value(1),
          "number of benchmarks to be started in parallel")
      ("repetitions,r", po::value<int>(&runs)->default_value(1),
          "number of repetitions of a benchmark")
      ("ssize", po::value<string>()->default_value("4K"),
          "size for sequential benchmarks in [B|K|M|G] "
          "(no modifier assumes bytes)")
      ("sw", po::bool_switch(&run_sw),
          "sequential write benchmark")
      ("chunk-size", po::value<string>()->default_value("128K"),
          "Chunk size of reads/writes in benchmark in [B|K|M|G] "
          "(no modifier assumes bytes)")
      ("stripe-size", po::value<string>()->default_value("128K"),
          "stripeSize in [B|K|M|G] (no modifier assumes bytes)")
      ("stripe-width", po::value<int>(&stripe_width)->default_value(1),
          "stripe width")
      ("dir",
          po::value<string>()->default_value("pbrpc://localhost:32638"),
          "URL to DIR")
      ("mrc",
          po::value<string>()->default_value("pbrpc://localhost:32636"),
          "URL to MRC")
      ("admin_password", po::value<string>(&admin_password),
          "administrator password to authorize operations");
}

void BenchmarkOptions::ParseCommandLine(int argc, char** argv) {
  // Parse general options and retrieve unregistered options for own parsing.
  vector<string> options = Options::ParseCommandLine(argc, argv);

  // Read volume names from command line.
  po::positional_options_description p;
  p.add("volume_names", -1);
  po::options_description positional_options("Volume names");
  positional_options.add_options()("volume_names",
                                   po::value<vector<string> >(&volume_names),
                                   "volumes used for benchmarking");

  // Parse command line.
  po::options_description all_descriptions;
  all_descriptions.add(benchmark_descriptions_).add(positional_options);
  po::variables_map vm;
  try {
    po::store(po::command_line_parser(options)
        .options(all_descriptions)
        .positional(p)
        .style(style::default_style & ~style::allow_guessing)
        .run(), vm);
    po::notify(vm);
  } catch(const std::exception& e) {
    // Rethrow boost errors due to invalid command line parameters.
    throw InvalidCommandLineParametersException(string(e.what()));
  }

  // Do not check parameters if the help shall be shown.
  if (show_help || empty_arguments_list || show_version) {
    return;
  }

  sequential_size = xtreemfs::parseByteNumber(vm["ssize"].as<string>());
  chunk_size = xtreemfs::parseByteNumber(vm["chunk-size"].as<string>());
  stripe_size = xtreemfs::parseByteNumber(vm["stripe-size"].as<string>());

  if (sequential_size < 0 || chunk_size < 0 || stripe_size < 0) {
    throw InvalidCommandLineParametersException("invalid size modifier");
  }

  if (!run_sw) {
    throw InvalidCommandLineParametersException(
            "benchmark type has to be specified");
  }

  if (volume_names.size() != num && volume_names.size() > 0) {
    throw InvalidCommandLineParametersException(
        "invalid number of volumes: has to be equal to -n");
  }

  create_volumes = (volume_names.size() == 0);

  // Parse addresses
  PBRPCURL dir_url_parser, mrc_url_parser;
  dir_url_parser.ParseURL(vm["dir"].as<string>(), PBRPCURL::GetSchemePBRPC(), DIR_PBRPC_PORT_DEFAULT);
  dir_address = dir_url_parser.GetAddresses();
  mrc_url_parser.ParseURL(vm["mrc"].as<string>(), PBRPCURL::GetSchemePBRPC(), MRC_PBRPC_PORT_DEFAULT);
  mrc_address = mrc_url_parser.GetAddresses();
}

std::string BenchmarkOptions::ShowCommandLineUsage() {
  return helptext_usage_
      + "\nFor complete list of options, please specify -h or --help.\n";
}

std::string BenchmarkOptions::ShowCommandLineHelp() {
  ostringstream stream;
  // No help text given in descriptions for positional options. Instead
  // the usage is explained here.
  stream << helptext_usage_
         << endl
         // Descriptions of this class.
         << benchmark_descriptions_
         // Descriptions of the general options.
         << endl
         << Options::ShowCommandLineHelp();
  return stream.str();
}

}  // namespace xtreemfs
