#include <cmath>
#include "raceInfo.hpp"

IntVec IntVec::operator+(IntVec &another) {
  return IntVec(x + another.x, y + another.y);
}

bool IntVec::operator==(const IntVec &another) const {
  return x == another.x && y == another.y;
}

bool IntVec::operator<(const IntVec &another) const {
  return y == another.y ?	// If y position is the same
    x > another.x :		// better to be more to the left
    y < another.y;
}

RaceCourse course;

//変更：raceInfo.hppで定義する--------
// PlayerState::PlayerState() {}
// PlayerState::PlayerState(Position p, Velocity v):
//   position(p), velocity(v) {}
//--------------

using Message = pair<boost::optional<int>, vector<string>>;

static Message readInt(unique_ptr<boost::process::ipstream>& in) {
  vector<string> msg;
  if (!in) {
    msg.push_back("Input stream is closed");
    return make_pair(boost::none, msg);
  }
  int num;
  string str;
  try {
    *in >> str;
    num = stoi(str);
  } catch (const logic_error& e) {
    string clipped;
    if (str.length() >= 100) {
      str = str.substr(0, 100) + "...";
      clipped = "(clipped)";
    }
    const string errmsg =
      typeid(e) == typeid(invalid_argument) ? "Invalid instruction" :
      typeid(e) == typeid(out_of_range) ? "Value out of range" :
      "Invalid instruction";
    msg.push_back(errmsg + "received from AI: \"" + str + "\"" + clipped);
    msg.push_back("\twhat: " + string(e.what()));
    return make_pair(boost::none, msg);
  }
  return make_pair(boost::optional<int>(num), msg);
}

static void logging(promise<void> promise, unique_ptr<istream> input, shared_ptr<ostream> output, shared_ptr<mutex> mtx, int MAX_SIZE) {
  if (output) {
    int size = 0;
    for (; MAX_SIZE == -1 || size < MAX_SIZE; ++size) {
      char c;
      if (input->get(c)) {
        lock_guard<mutex> lock(*mtx);
        *output << c;
      } else {
        break;
      }
    }
    if (MAX_SIZE != -1 && size >= MAX_SIZE) {
      *output << endl;
      *output << "[system] stderr output have reached the limit(MAX_SIZE=" << MAX_SIZE << " bytes)" << endl;
    }
  }
  promise.set_value();
}

static void handShake(unique_ptr<boost::process::ipstream> in, promise<pair<unique_ptr<boost::process::ipstream>, Message>> p)
{
  auto ans = readInt(in);
  p.set_value(make_pair(move(in), ans));
}

template <class... Args>
void sendToAI(unique_ptr<boost::process::opstream>&  toAI, shared_ptr<ofstream> stdinLogStream, const char *fmt, Args... args) {
  int n = snprintf(nullptr, 0, fmt, args...);
  unique_ptr<char[]> cstr(new char[n + 2]);
  memset(cstr.get(), 0, n + 2);
  snprintf(cstr.get(), n + 1, fmt, args...);
  string str(cstr.get());

  //debug
  // cout << "sent:" << endl;
  // cout << str;

  *toAI << str;
  if (stdinLogStream.get() != nullptr) {
    *stdinLogStream << str;
  }
}

void flushToAI(unique_ptr<boost::process::opstream>& toAI, shared_ptr<ofstream> stdinLogStream) {
  toAI->flush();
  if (stdinLogStream.get() != nullptr) stdinLogStream->flush();
}

Logger::Logger(unique_ptr<istream> input, shared_ptr<ostream> output, int MAX_SIZE = -1): mtx(new mutex) {
  promise<void> prms;
  ftr = prms.get_future();
  thrd = thread(logging, move(prms), move(input), output, mtx, MAX_SIZE);
}

Logger::~Logger() {
  mtx->unlock();
  future_status result = ftr.wait_for(chrono::milliseconds(500));
  if (result == future_status::timeout) {
    thrd.detach();
  } else {
    thrd.join();
  }
}

Player::Player(string command, string name, const RaceCourse &course, Position &mypos, Velocity &myvel, const Option &opt):
    name(name), group(), state(PlayerState(RACING,mypos,myvel, course.thinkTime)), option(opt)
{
  if (command.length() == 0) {
    state.state = ALREADY_DISQUALIFIED;
    return;
  }
  auto env = boost::this_process::environment();
  error_code error_code_child;
  unique_ptr<boost::process::ipstream> stderrFromAI(new boost::process::ipstream);
  toAI = unique_ptr<boost::process::opstream>(new boost::process::opstream);  
  #if defined(__unix__) || defined(__linux__)
	{
		// get native handle of writing
		const auto sink = toAI->pipe().native_sink();
		// calucate possible size
		int psize = 0;
		const int newline_size = sizeof(toAI->widen('\n'));
		psize += (int) (std::ceil(std::log10(course.thinkTime)) + 3 + newline_size) * (1 + course.stepLimit);
		psize += (int) (std::ceil(std::log10(course.stepLimit)) + newline_size) * (1 + course.stepLimit);
		psize += (int) (std::ceil(std::log10(course.width)) + 1) * (1 + course.stepLimit * 2);
		psize += (int) (std::ceil(std::log10(course.length)) + newline_size) * (1 + course.stepLimit * 2);
		psize += (int) (std::ceil(std::log10(course.vision)) + newline_size);
		psize += (int) (3 * course.width + newline_size) * course.length * course.stepLimit;
		// set pipe size
		const auto res = fcntl(sink, F_SETPIPE_SZ, psize);
		if (res < 0) {
			std::cerr << __FILE__ << ":" << __LINE__ << " F_SETPIPE_SZ failed... target size : " << psize << std::endl;
		}
	}
  #endif
  fromAI = unique_ptr<boost::process::ipstream>(new boost::process::ipstream);
  child = unique_ptr<boost::process::child>(new boost::process::child(
    command,
    boost::process::std_out > *fromAI,
    boost::process::std_err > *stderrFromAI,
    boost::process::std_in < *toAI,
    env,
    error_code_child,
    group
  ));
  if (option.stderrLogStream) {
    *option.stderrLogStream << "[system] Try: hand shake" << endl;
  }
  stderrLogger = unique_ptr<Logger>
    (new Logger(move(stderrFromAI), option.stderrLogStream, 1 << 15));
  sendToAI(toAI, option.stdinLogStream, "%d\n%d\n%d %d\n%d\n",
	   course.thinkTime, course.stepLimit,
	   course.width, course.length, course.vision);
  flushToAI(toAI, option.stdinLogStream);


  promise<pair<unique_ptr<boost::process::ipstream>, Message>> prms;
  future<pair<unique_ptr<boost::process::ipstream>, Message>> ftr = prms.get_future();
  chrono::milliseconds remain(state.timeLeft);
  thread thrd(handShake, move(fromAI), std::move(prms));
  chrono::system_clock::time_point start = chrono::system_clock::now();
  future_status result = ftr.wait_for(remain);
  chrono::system_clock::time_point end = chrono::system_clock::now();

  auto timeUsed = chrono::duration_cast<chrono::milliseconds>(end - start).count();
  state.timeLeft -= timeUsed;
  stderrLogger->mtx->lock();
  if (option.stderrLogStream) {
    *option.stderrLogStream << "[system] spend time: " << timeUsed << ", remain: " << state.timeLeft << endl;
  }
  if (option.pauseCommand) {
    error_code ec;
    int result = boost::process::system(boost::process::shell, option.pauseCommand.get(), ec, boost::process::std_out > stderr);
    cerr << __FILE__ << ":" << __LINE__ << ": [pause] (" << name << ") return code: " << result << ", error value: " << ec.value() << ", error message: " << ec.message() << endl;
  }
  if (result == future_status::timeout) {
    state.state = ALREADY_DISQUALIFIED;
    cerr << "player: \"" << name
	      << "\" did not respond in time during initiation" << endl;
    if (option.stderrLogStream) {
      *option.stderrLogStream << "your AI: \"" << name
			      << "\" did not respond in time during initiation"
			      << endl;
    }
    thrd.detach();
    return;
  }
  thrd.join();
  auto ret = ftr.get();
  fromAI = move(ret.first);
  auto ans = ret.second.first;

  for (const auto& line: ret.second.second) {
    cerr << line << endl;
    if (option.stderrLogStream) {
      *option.stderrLogStream << "[system] " << line << endl;
    }
  }
  if (!ans || ans.get() != 0) {
    if (option.stderrLogStream) {
      *option.stderrLogStream << "[system] Failed...: hand shake" << endl;
    }
    if (!child->running()) {
      cerr << "player: \"" << name << "\" died." << endl;
      cerr << "\texit code: " << child->exit_code() << endl;
      if (option.stderrLogStream) {
        *option.stderrLogStream << "[system] your AI: \"" << name << "\" died." << endl;
        *option.stderrLogStream << "[system] \texit code: " << child->exit_code() << endl;
      }
      state.state = ALREADY_DISQUALIFIED;
      return;
    }
    if (ans) {
      int v = ans.get();
      cerr << "Response at initialization of player \"" << name << "\": ("
     << v << ") is non-zero" << endl;
      if (option.stderrLogStream) {
        *option.stderrLogStream << "[system] Response at initialization of player \"" << name << "\": (" << v << ") is non-zero" << endl;
      }
    }
    state.state = ALREADY_DISQUALIFIED;
  } else {
    if (option.stderrLogStream) {
      *option.stderrLogStream << "[system] Success!: hand shake" << endl;
    }
  }

  //debug
  // std::cout << "answer:" << ans.get() << std::endl;
}

//---------------------------







void addSquares(int x, int y0, int y1, list <Position> &squares) {
  if (y1 > y0) {
    for (int y = y0; y <= y1; y++) {
      squares.emplace_back(x, y);
    }
  } else {
    for (int y = y0; y >= y1; y--) {
      squares.emplace_back(x, y);
    }
  }
}


list <Position> Movement::touchedSquares() const {
  list <Position> r;
  int dx = to.x - from.x;//x方向(右側)に進んだ距離
  int dy = to.y - from.y;//y方向(上側)に進んだ距離
  int sgnx = dx > 0 ? 1 : -1;//右に進んだなら1，それ以外は-1
  int sgny = dy > 0 ? 1 : -1;//上に進んだなら1，それ以外は-1
  int adx = abs(dx);//x方向に進んだ距離の絶対値
  int ady = abs(dy);//y方向に進んだ距離の絶対値
  if (dx == 0) {//もしx方向に進んでいないなら
    for (int k = 0, y = from.y; k <= ady; k++, y += sgny) {//それまでに通ったマスをrに記録する
      r.emplace_back(from.x, y);
    }
  } else if (dy == 0) {//同様にそれまでに通ったマスをrに記録する
    for (int k = 0, x = from.x; k <= adx; k++, x += sgnx) {
      r.emplace_back(x, from.y);
    }
  } else {//同様にそれまでに通ったマスをrに記録する
    // Let us transform the move line so that it goes up and to the right,
    // that is, with dx > 0 and dy > 0.
    // The results will be adjusted afterwards.
    if (sgnx < 0) dx = -dx;
    if (sgny < 0) dy = -dy;
    // We will use the coordinate system in which the start point
    // of the move is at (0,0) and x-coodinate values are doubled,
    // so that x is integral on square borders.
    // The point (X,Y) in the original coordinate system becomes
    //   x = 2*(X-from.x)
    //   y = Y-from.y
    // Such a movement line satisfies the following.
    //   y = dy/dx/2 * x, or 2*dx*y = dy*x
    //
    // The start square and those directly above it
    for (int y = 0; dx*(2*y-1) <= dy; y++) {
      r.emplace_back(0, y);
    }
    // The remaining squares except for those below (dx, dy)
    for (int x = 1; x < 2*dx-1; x += 2) {
      for (int y = (dy*x+dx)/(2*dx) -
	     (dy*x+dx == (dy*x+dx)/(2*dx)*(2*dx) ? 1 : 0);
	   dx*(2*y-1) <= dy*(x+2);
	   y++) {
	r.emplace_back((x+1)/2, y);
      }
    }
    // For the final squares with x = dx
    for (int y = (dy*(2*dx-1)+dx)/(2*dx) -
	   ((dy*(2*dx-1)+dx) == (dy*(2*dx-1)+dx)/(2*dx)*(2*dx) ? 1 : 0);
	 y <= dy;
	 y++) {
      r.emplace_back(dx, y);
    }
    // Adjustment
    for (auto &p: r) {
      if (sgnx < 0) p.x = -p.x;
      if (sgny < 0) p.y = -p.y;
      p.x += from.x;
      p.y += from.y;
    }
  }
  return r;
}

istream &operator>>(istream &in, RaceCourse &course) {
  in >> course.thinkTime
     >> course.stepLimit
     >> course.width >> course.length
     >> course.vision;
  return in;
}

istream &operator>>(istream &in, PlayerState &ps) {
  in >> ps.position.x
     >> ps.position.y
     >> ps.velocity.x
     >> ps.velocity.y;
  return in;
};

istream &operator>>(istream &in, RaceInfo &ri) {
  in >> ri.stepNumber
     >> ri.timeLeft
     >> ri.me
     >> ri.opponent;
  //変更：動的メモリ確保はこの演算子の前にする
  //この演算子の度にメモリ確保するのは非効率的だと思ったため
  // ri.squares = new char*[course.length];
  for (int y = 0; y != course.length; y++) {
    // ri.squares[y] = new char[course.width];
    for (int x = 0; x != course.width; x++) {
      int state;
      in >> state;
      ri.squares[y][x] = state;
    }
  }
  return in;
}




//--- from player.cpp
using Message4Act = pair<boost::optional<pair<int, int>>, vector<string>>;


static void readAct(unique_ptr<boost::process::ipstream> in, promise<pair<unique_ptr<boost::process::ipstream>, Message4Act>> p)
{
  auto ax = readInt(in);
  auto ay = readInt(in);

  vector<string> msg;
  msg.insert(msg.end(), ax.second.begin(), ax.second.end());
  msg.insert(msg.end(), ay.second.begin(), ay.second.end());
  if (ax.first && ay.first) {
    p.set_value(
      make_pair(
        move(in),
        make_pair(
          boost::optional<pair<int, int>>(make_pair(ax.first.get(), ay.first.get())),
          msg
        )
      )
    );
  } else {
    p.set_value(make_pair(move(in), make_pair(boost::none, msg)));
  }
}

ResultCategory Player::
plan(int stepNumber, RaceInfo &info, RaceCourse &course,
     Acceleration &accel, int64_t &timeUsed) {
  if (option.stderrLogStream) {
    *option.stderrLogStream << "[system] ================================" << endl;
    *option.stderrLogStream << "[system] turn: " << stepNumber << endl;
  }

  sendToAI(toAI, option.stdinLogStream, "%d\n%" PRId64 "\n%d %d %d %d\n",
	   stepNumber, info.timeLeft,
	   info.me.position.x, info.me.position.y,
	   info.me.velocity.x, info.me.velocity.y);

  //変更：プレイヤーの状態を考慮しない
  //we have no state, so we just send info
  sendToAI(toAI, option.stdinLogStream, "%d %d %d %d\n",
      info.opponent.position.x, 
      info.opponent.position.y,
      info.opponent.velocity.x, 
      info.opponent.velocity.y);

  for (int y = 0; y < course.length; y++) {
    for (int x = 0; x < course.width; ++x) {
      if (x != 0) {
        sendToAI(toAI, option.stdinLogStream, " ", 0);
      }
      sendToAI(toAI, option.stdinLogStream, "%d",info.squares[y][x]);//modified
    }
    sendToAI(toAI, option.stdinLogStream, "\n", 0);
  }
  flushToAI(toAI, option.stdinLogStream);

  promise<pair<unique_ptr<boost::process::ipstream>, Message4Act>> prms;
  future<pair<unique_ptr<boost::process::ipstream>, Message4Act>> ftr = prms.get_future();
  stderrLogger->mtx->unlock();
  if (option.resumeCommand) {
    error_code ec;
    int result = boost::process::system(boost::process::shell, option.resumeCommand.get(), ec, boost::process::std_out > stderr);
    cerr << __FILE__ << ":" << __LINE__ << ": [resume] (" << name << ") return code: " << result << ", error value: " << ec.value() << ", error message: " << ec.message() << endl;
  }
  chrono::milliseconds remain(state.timeLeft);

  thread thrd(readAct, move(fromAI), std::move(prms));
  chrono::system_clock::time_point start = chrono::system_clock::now();
  future_status result = ftr.wait_for(remain);
  chrono::system_clock::time_point end = chrono::system_clock::now();
  timeUsed = chrono::duration_cast<chrono::milliseconds>(end - start).count();
  state.timeLeft -= timeUsed;
  stderrLogger->mtx->lock();
  if (option.stderrLogStream) {
    *option.stderrLogStream << "[system] spend time: " << timeUsed
			    << ", remain: " << state.timeLeft << endl;
  }
  if (option.pauseCommand) {
    error_code ec;
    int result = boost::process::system(boost::process::shell, option.pauseCommand.get(), ec, boost::process::std_out > stderr);
    cerr << __FILE__ << ":" << __LINE__ << ": [pause] (" << name << ") return code: " << result << ", error value: " << ec.value() << ", error message: " << ec.message() << endl;
  }

  //変更：時間を考慮しない
  // if (result == future_status::timeout) {
  //   cerr << "player: " << name
	//       << " did not respond in time at step "
	//       << stepNumber << endl;
  //   if (option.stderrLogStream) {
  //     *option.stderrLogStream
	// << "[system] your AI: \""
	// << name << "\" did not respond in time at step "
	// << stepNumber << endl;
  //   }
  //   thrd.detach();
  //   return TIMEDOUT;
  // }
  thrd.join();
  auto ret = ftr.get();
  fromAI = move(ret.first);

  for (const auto& line: ret.second.second) {
    cerr << line << endl;
    if (option.stderrLogStream) {
      *option.stderrLogStream << "[system] " << line << endl;
    }
  }
  if (ret.second.first) {
    auto val = ret.second.first.get();
    if (val.first < -1 || 1 < val.first
      || val.second < -1 || 1 < val.second) {
      cerr << "acceleration value must be from -1 to 1 each axis, but player: \"" << name << "\" saied: (" << val.first << ", " << val.second << ")" << endl;
      if (option.stderrLogStream) {
        *option.stderrLogStream << "[system] acceleration value must be from -1 to 1 each axis, but your AI: \"" << name << "\" saied: (" << val.first << ", " << val.second << ")" << endl;
      }
      return INVALID;
    }
    accel = Acceleration(val.first, val.second);

    //debug
    cout << "accel set:" << accel.x << "," << accel.y << endl;

    return NORMAL;
  } else {
    //debug
    cout << "Not NORMAL" << endl;

    if (!child->running()) {
      cerr << "player: \"" << name << "\" died." << endl;
      cerr << "\texit code: " << child->exit_code() << endl;
      if (option.stderrLogStream) {
        *option.stderrLogStream << "[system] your AI: \"" << name << "\" died." << endl;
        *option.stderrLogStream << "[system] \texit code: " << child->exit_code() << endl;
      }
      return DIED;
    }
    return INVALID;
  }
}
