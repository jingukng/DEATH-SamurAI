#ifndef RACEINFO_INCLUDE
#define RACEINFO_INCLUDE

#include <iostream>
#include <utility>
#include <list>

//-- from player.hpp --//
#include <cstdio>
#include <memory>
#include <thread>
#include <istream>
#include <ostream>
#include <boost/process.hpp>
#include <boost/optional.hpp>
//---------------------

#include <cstdint>
#include <cinttypes>

#if defined(__unix__) || defined(__linux__)
#include <fcntl.h>
#include <cmath>
#endif

//デバッグ用．ステップの数を少なくして確かめる時につかう
#define DEBUG_STEPLIMIT 5

using namespace std;
using namespace rel_ops;

struct IntVec {
  int x, y;
  IntVec operator+(IntVec &another);
  bool operator==(const IntVec &another) const;
  bool operator<(const IntVec &another) const;
  IntVec(int x = 0, int y = 0): x(x), y(y) {};
};

typedef IntVec Position;
typedef IntVec Velocity;
typedef IntVec Acceleration;

struct RaceCourse {
  uint64_t thinkTime;
  int stepLimit;
  int width, length;
  int vision;
};

extern RaceCourse course;

struct Movement {
  Position from, to;
  bool goesOff(RaceCourse &course);
  list <Position> touchedSquares() const;
  Movement() {};
  Movement(Position from, Position to): from(from), to(to) {};
};

//-- from player.hpp
enum PlayerStateCategory {
  RACING, ALREADY_FINISHED, ALREADY_DISQUALIFIED
};

struct PlayerState {
  PlayerStateCategory state;
  Position position;
  Velocity velocity;
  int64_t timeLeft;
  PlayerState(PlayerStateCategory s, Position p, Velocity v,
	      int64_t timeLeft):
    state(s), position(p), velocity(v), timeLeft(timeLeft) {};
  PlayerState() {};
  PlayerState(Position p, Velocity v):
    position(p), velocity(v) {};
};
//-------------------

struct RaceInfo {
  int stepNumber;
  uint64_t timeLeft;
  PlayerState me, opponent;
  char **squares;

};


//---- from player.hpp

struct Option {
  std::shared_ptr<std::ofstream> stdinLogStream;
  std::shared_ptr<std::ofstream> stderrLogStream;
  boost::optional<std::vector<std::string>> pauseCommand;
  boost::optional<std::vector<std::string>> resumeCommand;
};

class Logger {
private:
  thread thrd;
  future<void> ftr;
public:
  shared_ptr<mutex> mtx;
  Logger(unique_ptr<istream> input, shared_ptr<ostream> output, int MAX_SIZE);
  ~Logger();
};

enum ResultCategory {
  NORMAL, FINISHED,		// Normal run
  GONEOFF, OBSTACLED, COLLIDED,	// Ran but with problem
  NOPLAY,			// Already stopped running
  TIMEDOUT, DIED, INVALID	// Problem in response
};

struct Player {
  // Player AI
  string name;
  boost::process::group group;
  PlayerState state;
  Option option;

  unique_ptr<boost::process::child> child;
  unique_ptr<boost::process::opstream> toAI;
  unique_ptr<boost::process::ipstream> fromAI;
  unique_ptr<Logger> stderrLogger;
  // Player state during a race

  Player() {};
  // Player(string command, string name, const RaceCourse &course, int xpos, const Option &opt);
  Player(string command, string name, const RaceCourse &course, Position &mypos, Velocity &myvel, const Option &opt);
  bool playOneStep (int stepNumber, RaceInfo &info, RaceCourse &course);
  ResultCategory plan (int stepNumber, RaceInfo &info, RaceCourse &course, Acceleration &accel, int64_t &timeUsed);
  // void terminate();
};
//---------


istream &operator>>(istream &in, RaceCourse &course);
istream &operator>>(istream &in, PlayerState &ps);
istream &operator>>(istream &in, RaceInfo &ri);

#endif
