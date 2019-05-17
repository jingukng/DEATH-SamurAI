#include "raceState.hpp"

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#if defined(__unix__) || defined(__linux__)
#include <csignal>
#endif

int main(int argc, char *argv[]) {
#if defined(__unix__) || defined(__linux__)
  std::signal(SIGPIPE, SIG_IGN);
#endif
  boost::program_options::options_description desc("<option>");
  desc.add_options()
    ("stdinLogFile0", boost::program_options::value<std::string>()->value_name("<filename>"), "logfile name for dump data that this program outputs to player0's standard input")
    ("stdinLogFile1", boost::program_options::value<std::string>()->value_name("<filename>"), "logfile name for dump data that this program outputs to player1's standard input")
    ("stderrLogFile0", boost::program_options::value<std::string>()->value_name("<filename>"), "logfile name for error information with the output of player0's standard error")
    ("stderrLogFile1", boost::program_options::value<std::string>()->value_name("<filename>"), "logfile name for error information with the output of player1's standard error")
    ("pauseP0", boost::program_options::value<std::string>()->value_name("<command>"), "pause command for player0")
    ("resumeP0", boost::program_options::value<std::string>()->value_name("<command>"), "resume command for player0")
    ("pauseP1", boost::program_options::value<std::string>()->value_name("<command>"), "pause command for player1")
    ("resumeP1", boost::program_options::value<std::string>()->value_name("<command>"), "resume command for player1")
    ("help,h", "produce help");
  boost::program_options::variables_map vm;
  std::vector<std::string> unnamed_args;
  auto help_message = [&] {
    //変更；名前の代わりにポート番号を指定
    cerr << "Usage: " << argv[0] << " <course file> <player0> <port0> <player1> <port1> [<option>...]" << endl;
    cerr << endl;
    cerr << desc << endl;
  };
  try {
    const auto parsing_result = boost::program_options::parse_command_line(argc, argv, desc);
    boost::program_options::store(parsing_result, vm);
    boost::program_options::notify(vm);
    unnamed_args = boost::program_options::collect_unrecognized(parsing_result.options, boost::program_options::include_positional);
  } catch (std::exception& e) {
    cerr << "invalid command line arguments" << endl;;
    cerr << e.what() << endl;
    help_message();
    return 1;
  }
  if (vm.count("help") || unnamed_args.size() != 5) {
    help_message();
    return 1;
  }
  ifstream in(unnamed_args[0]);
  RaceCourse course(in);
  string player0 = unnamed_args[1];

  //変更：ポート番号を文字列で受け取る
  string port0 = unnamed_args[2];

  string player1 = unnamed_args[3];

  //変更：ポート番号を文字列で受け取る
  string port1 = unnamed_args[4];

  std::array<Option, 2> options;
  auto take_ofstream = [](const boost::program_options::variables_map& vm, const std::string& target) -> std::shared_ptr<std::ofstream> {
    if (vm.count(target)) {
      return std::shared_ptr<std::ofstream>(new std::ofstream(vm[target].as<std::string>()));
    } else {
      return nullptr;
    }
  };
  auto take_command = [](const boost::program_options::variables_map& vm, const std::string& target) -> boost::optional<std::vector<std::string>> {
    if (vm.count(target)) {
      std::vector<std::string> args;
      boost::algorithm::split(args, vm[target].as<std::string>(), boost::is_space(), boost::algorithm::token_compress_on);
      return args;
    } else {
      return boost::none;
    }
  };
  for (int i = 0; i < 2; ++i) {
    options[i].stdinLogStream = take_ofstream(vm, "stdinLogFile" + std::to_string(i));
    options[i].stderrLogStream = take_ofstream(vm, "stderrLogFile" + std::to_string(i));
    options[i].pauseCommand = take_command(vm, "pauseP" + std::to_string(i));
    options[i].resumeCommand = take_command(vm, "resumeP" + std::to_string(i));
  }

  //変更：先にraceStateでプレイヤーを作成し，名前を得る
  RaceState raceState(course, player0, port0, player1, port1, options);
  //名前をクライアントから受け取って，logを開始する
  string name0 = raceState.players[0].name;
  string name1 = raceState.players[1].name;

  RaceLog log(course, name0, name1);

  for (int p = 0; p != 2; p++) {
    raceState.visibility = course.vision;
  }
  
  for (int stepNumber = 0;
       stepNumber != course.stepLimit 
       &&
	 !log.playOneStep(stepNumber, raceState);
       stepNumber++) {
  }
  for (int p = 0; p != 2; p++) {
    raceState.players[p].terminate();
  }
  cout << log;
  return 0;
}
