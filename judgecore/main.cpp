#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <map>

#include <signal.h>
#include <unistd.h>

#include <seccomp.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "defs.h"

#include "json.hpp"
using json = nlohmann::json;

using namespace std;

bool debug = false;

json result;

map<string, string> path;

enum RESULT {AC = 0, PE, WA, CE, RE, ME, TE, OLE, SLE, SW};

int create_folder(string& path) {
    const int error = system((string("mkdir -p ") + path).c_str ());
    if (error != 0)
    {
        cerr << "create directory at " << path << " failed" << endl;
        return -1;
    }
    return 0;
}

int delete_file(string& path) {
    const int error = system((string("rm -rf ") + path).c_str ());
    if (error != 0)
    {
        cerr << "delete file at " << path << " failed" << endl;
        return -1;
    }
    return 0;
}

std::string readFile(const string& filename) {
  ifstream in(filename);
  return static_cast<std::stringstream const&>(std::stringstream() << in.rdbuf()).str();
}

string getStatusText(RESULT what) {
  switch (what)
  {
    case SW:
      return "system error";
      break;
    case CE:
      return "compile error";
    case RE: 
      return "runtime error";
    case TE:
      return "time limit exceed";
    case ME:
      return "memory limit exceed";
    case OLE:
      return "output limit exceed";
    case SLE:
      return "syscall not allowed";
    case WA:
      return "wrong answer";
    case PE:
      return "presentation error";
    case AC:
      return "accepted";
    default:
      return "unknown";
  }
}

int finish(RESULT what) {
  result["status"] = what;
  result["result"] = getStatusText(what);
  ofstream of(path["result"]);
  of << (debug ? setw(2) : setw(0)) << result << endl;
  _exit(0);
}

int read_config(int argc, char** argv, json& j) {
  bool has_stdin = false;

  if (argc < 2) {
    cerr << "usage: judge [config.json [stdin]]" << endl;
    return -1;
  }

  for (int i = 1; i < argc; i++) {
    json temp;
    if (strncmp(argv[i], "stdin", 5) == 0) {
      if (!has_stdin) {
        has_stdin = true;
        try {
          cin >> temp;
        } catch (...) {
          cerr << "failed to parse json from stdin" << endl;
          return -1;
        }
      } else {
        cerr << "could only have one stdin" << endl;
        return -1;
      }
    } else {
      ifstream fin(argv[i]);
      if (!fin.is_open()) {
        cerr << "could not open file '"<< argv[i] << "' (at argv " << i << ") for read" << endl;
        return -1;
      }
      try {
        fin >> temp;
      } catch (...) {
        cerr << "failed to parse json from file '"<< argv[i] << "' (at argv " << i << ")" << endl;
        return -1;
      }
    }
    j.merge_patch(temp);
  }

  if (j["debug"].is_boolean() && j["debug"]) {
    debug = true;
  }

  if (debug) {
    cout << setw(4) << j << endl;
  }
  return 0;
}

int validate_config(json& j) {
  // TODO: valid configuration
  if (debug) 
    cout << "[warn] validate configuration is skipped." << endl;
  return 0;
}

int comile_c_cpp(json& j, const string& compile_command) {
  if (debug) 
    cout << "compiler command: " << compile_command << endl;    

  pid_t pid = fork();
  if(pid == -1) {
      cerr << "fork complier process failed" << endl;
      return -1;
  }
  if(pid == 0) {
      alarm(10);
      signal(SIGALRM, [](int sig){exit(-1);});
      int ret = system(compile_command.c_str ());
      unsigned int sec = 10 - alarm(0);
      if (debug)
        cout << "compile time is " << sec << " seconds" << endl;
      if(WIFEXITED(ret))
          exit(WEXITSTATUS(ret));
      raise(WTERMSIG(ret));
  } else {
      int status;
      wait(&status);
      if(!WIFEXITED(status)) {
          cerr << "compiler process killed by sig " << WTERMSIG(status) << endl;
          return WTERMSIG(status);
      }
      status = WEXITSTATUS(status);
      if (debug)
        cout << "compiler return code is : " << status << endl;
      result["compiler"] = readFile(path["cmpinfo"]);
      if(status != 0) {
        finish (CE);
      }
      return 0;
  }
  // should not
  raise(SIGTRAP);
  return -1;
}

int compile_exec_c (json& j) {
  if (debug) 
    cout << "language is c" << endl;
  string compile_command = "gcc -DONLINE_JUDGE -O2 -std=c11 -fno-asm -Wall -Wextra -o "
          + path["exec"] + " " + path["code"] + " >"
          + path["cmpinfo"] + " 2>&1";
  return comile_c_cpp(j, compile_command);
}

int compile_exec_cpp (json& j) {
  if (debug) 
    cout << "language is cpp" << endl;
  string compile_command = "g++ -DONLINE_JUDGE -O2 -std=c++14 -fno-asm -Wall -Wextra -o "
          + path["exec"] + " " + path["code"] + " >"
          + path["cmpinfo"] + " 2>&1";
  return comile_c_cpp(j, compile_command);
}

int compile_exec_javascript (json& j) {
  if (debug) 
    cout << "language is javascript, skip compile" << endl;
  ofstream script(path["exec"] + ".nodejs");
  string s = R"+(const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: false,
  crlfDelay: Infinity,
});

const line = (async function* _readLine() {
  for await (const line of rl) {
    yield line;
  }
})()

async function read() {
  return (await line.next()).value
}

function write(data) {
  process.stdout.write(data.toString())
}

function writeLine(data) {
  process.stdout.write(data + '\n')
}

process.on('unhandledRejection', (reason, p) => {
  console.log('Unhandled Rejection at: Promise', p, 'reason:', reason)
  process.exit(-1)
});

(async function main() {
)+"
+ readFile(path["code"]) +
R"+(
})().then(() => process.exit(0)))+";  
  script << s << endl;
  script.close();
  
  ofstream exec(path["exec"]);
  exec << "#! /bin/bash\n";
  exec << "exec node --no-warnings " + path["exec"] + ".nodejs" << endl;
  exec.close();
  return 0;
}

int compile_exec_python (json& j) {
  ofstream script(path["temp"] + "compile_script.py");
  script << "import py_compile\npy_compile.compile('";
  script << path["code"] << "', cfile='" << path["exec"];
  script << ".pyc', doraise=True)" << endl;
  script.close();
  string compile_command = "python3 -OO "
          + path["temp"] + "compile_script.py" + " >"
          + path["cmpinfo"] + " 2>&1";
  int r = comile_c_cpp(j, compile_command);
  if (r) return r;
  ofstream exec(path["exec"]);
  exec << "#! /bin/bash\n";
  exec << "exec python3 " + path["exec"] + ".pyc" << endl;
  exec.close();
  return 0;
}

int compile_exec_go (json& j) {
  string compile_command = "go build -o " + path["exec"] 
          + " " + path["code"] + " >" + path["cmpinfo"] + " 2>&1";
  return comile_c_cpp(j, compile_command);
}

int compile_exec_custom (json& j) {
  cerr << "lang not specific or unknown language" << endl;
  return -1;
}

int generate_exec_args (json& j) {
  if (j["lang"].is_string()) {
    if(j["lang"].get<string>() == "c") {
      return compile_exec_c(j);
    } else if(j["lang"].get<string>() == "c++") {
      return compile_exec_cpp(j);
    } else if(j["lang"].get<string>() == "javascript") {
      return compile_exec_javascript(j);
    } else if(j["lang"].get<string>() == "python") {
      return compile_exec_python(j);
    } else if(j["lang"].get<string>() == "go") {
      return compile_exec_go(j);
    } else {
      return compile_exec_custom(j);
    }
  } else if (j["lang"].is_number_integer()) {
    if(j["lang"].get<int>() == 0) {
      return compile_exec_c(j);
    } else if(j["lang"].get<int>() == 1) {
      return compile_exec_cpp(j);
    } else if(j["lang"].get<int>() == 2) {
      return compile_exec_javascript(j);
    } else if(j["lang"].get<int>() == 3) {
      return compile_exec_python(j);
    } else if(j["lang"].get<int>() == 4) {
      return compile_exec_c(j);
    } else if(j["lang"].get<int>() == 5) {
      return compile_exec_go(j);
    } else {
      return compile_exec_custom(j);
    }
  } else {
    return -1;
  }
}

int load_seccomp(int level, const std::initializer_list<string>& exes) {
  setuid(99);
  setgid(99);
  nice(10);
  if (level == 0) {
    int syscalls_blacklist[] = {
      SCMP_SYS(clone),
      SCMP_SYS(fork),
      SCMP_SYS(vfork),
      SCMP_SYS(kill), 
#ifdef __NR_execveat
      SCMP_SYS(execveat)
#endif
    };
    int syscalls_blacklist_length = sizeof(syscalls_blacklist) / sizeof(int);
    scmp_filter_ctx ctx = NULL;
    // load seccomp rules
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) {
        return -1;
    }
    for (int i = 0; i < syscalls_blacklist_length; i++) {
        if (seccomp_rule_add(ctx, SCMP_ACT_TRAP, syscalls_blacklist[i], 0) != 0) {
            return -1;
        }
    }
    // use SCMP_ACT_KILL for socket, python will be killed immediately
    if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), SCMP_SYS(socket), 0) != 0) {
        return -1;
    }
    for (const auto& i : exes) {
      // add extra rule for execve
      if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t)(i.c_str()))) != 0) {
          return -1;
      }
    }
    // do not allow "w" and "rw" using open
    if (seccomp_rule_add(ctx, SCMP_ACT_TRAP, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) != 0) {
        return -1;
    }
    if (seccomp_rule_add(ctx, SCMP_ACT_TRAP, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) != 0) {
        return -1;
    }
    // do not allow "w" and "rw" using openat
    if (seccomp_rule_add(ctx, SCMP_ACT_TRAP, SCMP_SYS(openat), 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) != 0) {
        return -1;
    }
    if (seccomp_rule_add(ctx, SCMP_ACT_TRAP, SCMP_SYS(openat), 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) != 0) {
        return -1;
    }
    if (seccomp_load(ctx) != 0) {
        return -1;
    }
    seccomp_release(ctx);
    return 0;
  } else {
    int syscalls_whitelist[] = {
      SCMP_SYS(read), SCMP_SYS(fstat),
      SCMP_SYS(mmap), SCMP_SYS(mprotect),
      SCMP_SYS(munmap), SCMP_SYS(uname),
      SCMP_SYS(arch_prctl), SCMP_SYS(brk),
      SCMP_SYS(access), SCMP_SYS(exit_group),
      SCMP_SYS(close), SCMP_SYS(readlink),
      SCMP_SYS(sysinfo), SCMP_SYS(write),
      SCMP_SYS(writev), SCMP_SYS(lseek)
    };

    int syscalls_whitelist_length = sizeof(syscalls_whitelist) / sizeof(int);
    scmp_filter_ctx ctx = NULL;

    ctx = seccomp_init(SCMP_ACT_TRAP);
    if (!ctx) {
        return -1;
    }
    for (int i = 0; i < syscalls_whitelist_length; i++) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls_whitelist[i], 0) != 0) {
            return -1;
        }
    }

    for (const auto& i : exes) {
      // add extra rule for execve
      if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t)(i.c_str()))) != 0) {
          return -1;
      }
    }

    // do not allow "w" and "rw"
    if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
        return -1;
    }
    if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
        return -1;
    }
    if (seccomp_load(ctx) != 0) {
        return -1;
    }
    seccomp_release(ctx);
    return 0;
  }
  return -1;
}

bool should_continue(json& j, RESULT r) {
  if (j["on_error_continue"].is_boolean())
    return true;
  if (j["on_error_continue"].is_array()) {
    for (const auto& element : j["on_error_continue"]) {
      if (element.is_string()) {
        if (getStatusText(r) == element.get<string>())
          return true;
      } else if (element.is_number_integer()) {
        if (static_cast<int>(r) == element.get<int>())
          return true;
      } else {
        cerr << "[warn] unknown continue rules" << endl;
      }
    }
    return false;
  }
  cerr << "[warn] unknown continue rules" << endl;
  return true;
}

RESULT do_compare(json& j, const map<string, string>& extra) {
  if (j["spj_mode"].is_string() && j["spj_mode"].get<string>() == "compare"
    || j["spj_mode"].is_number_integer() && j["spj_mode"].get<int>() == 1) {
    // should do spj
    string spj_exec_path = 
      j["spj_exec"].is_string() ? j["spj_exec"].get<string>() : path["spj"];
    string spjcmd = spj_exec_path + " " + extra.at("stdin") + " " 
      + extra.at("stdout") + " " + extra.at("execout") 
      + " >" + extra.at("diff") + " 2>&1";
    if (debug)
      cout << "special judge command: " << spjcmd << endl;
    int status = system(spjcmd.c_str());
    if (debug)
      cout << "special judge returned: " << status << endl;
    if (WIFEXITED(status)) {
      switch  (WEXITSTATUS(status)) {
        case 0: return AC;
        case 1: return PE;
        case 2: return WA;
        default: 
          cerr << "[warn] special judge returned unknown status" << endl;
          return SW;
      }
    }
    cerr << "special judge program is signaled" << endl;
    return SW; 
  } else {
    string difcmd = string("diff ") + extra.at("stdout") + " " + extra.at("execout") + " >" + extra.at("diff");
    int status = system(difcmd.c_str());

    if (WEXITSTATUS(status) == 0)
      return AC;

    string difcmd = string("diff --ignore-space-change --ignore-all-space --ignore-blank-lines --ignore-case --brief ") + extra.at("stdout") + " " + extra.at("execout") + " >" + extra.at("diff");
    status = system(difcmd.c_str());

    if (WEXITSTATUS(status) == 0)
      return PE;
    return WA;
  }
  return SW;
}

int do_interactive_test(json& j) {

}

int do_test(json& j) {
  int time_limit = j["max_time"].get<int>();
  int total_time_limit = j["max_time_total"].get<int>();
  int memory_limit = j["max_memory"].get<int>();
  int output_limit = j["max_output"].get<int>();
  bool on_error_continue = j["on_error_continue"].get<bool>();

  int cases = j["test_case_count"].get<int>();

  int total_time = 0;
  int max_memory = 0;

  RESULT fatal_status = AC;

  struct {
    int time;
    int memory;
    int signal;
    int exitcode;
    RESULT result;
  } case_result;

  for (int c = 1; c <= cases; c++) {
    string cs = to_string(c);

    map<string, string> extra;
    extra["stdin"] = path["stdin"] + "/" + cs + ".in";
    extra["stdout"] = path["stdout"] + "/" + cs + ".out";
    extra["output"] = path["result"] + "/" + cs + ".execout";
    extra["diff"] = path["diff"] + "/" + cs + ".diff";

    if (debug) {
      cout << "test case " << cs << endl;
      cout << "input " << extra["stdin"] << endl;
    }

    int pid;

    if ((pid = fork()) < 0) {
      cerr << "fork for judge process failed" << endl;
      finish(SW);
    }

    if (pid == 0) { // child process
      rlimit rlimits;
      int r;

      rlimits.rlim_cur = time_limit / 1000 + 1;
      rlimits.rlim_max = min(time_limit / 1000 * 2, time_limit / 1000 + 4);
      if (r = setrlimit(RLIMIT_CPU, &rlimits)) {
        cerr << "set cpu time limit failed, " << r << endl;
        finish(SW);
      }

      rlimits.rlim_cur = memory_limit * 1024 * 2;
      rlimits.rlim_max = memory_limit * 1024 * 2 * 2;
      if (r = setrlimit(RLIMIT_AS, &rlimits)) {
        cerr << "set memory limit failed, " << r << endl;
        finish(SW);
      }

      rlimits.rlim_cur = output_limit;
      rlimits.rlim_max = output_limit * 2;
      if (r = setrlimit(RLIMIT_FSIZE, &rlimits)) {
        cerr << "set output limit failed, " << r << endl;
        finish(SW);
      }

      if (debug)
        cout << "execution begin" << endl;
      
      FILE *file_in = freopen(extra["stdin"].c_str(), "r", stdin);
      FILE *file_out = freopen(extra["execout"].c_str(), "w", stdout);

      if (file_in == nullptr || file_out == nullptr) {
        fclose(file_in);
        fclose(file_out);
        cerr << "failed to redirect input & output." << endl;
        finish(SW);
      }

      if(r = load_seccomp(0, {path["exec"]})) {
        cerr << "load seccomp rule failed" << endl;
        return;
      };

      execlp(path["exec"].c_str(), path["exec"].c_str(), nullptr);

      fclose(file_in);
      fclose(file_out);

      cerr << "exec failed" << endl;
      _exit(255);
    } else { // parent
      int status;
      struct rusage resource_usage;

      if (wait4(pid, &status, WSTOPPED, &resource_usage) == -1) {
        kill(pid, SIGKILL);
        cerr << "wait on child process failed" << endl;
        finish(SW);
      }

      json r;

      if (WIFSIGNALED(status) != 0) {
        if (debug) 
          cout << "user program is signaled" << endl;
        r["signal"] = case_result.signal = WTERMSIG(status);
      } else {
        case_result.signal = 0;
      }

      r["exitcode"] = case_result.exitcode = WEXITSTATUS(status);
      r["time"] = case_result.time = (int) (resource_usage.ru_utime.tv_sec * 1000 +
                                  resource_usage.ru_utime.tv_usec / 1000);
      r["memory"] = case_result.memory = resource_usage.ru_maxrss * 1024;

      RESULT rs;
      if (case_result.signal) {
        switch (case_result.signal)
        {
        case SIGXCPU: rs = TE; break;
        case SIGXFSZ: rs = OLE; break;
        case SIGSEGV: rs = RE; break;
        case SIGABRT: rs = ME; break;
        case SIGFPE : rs = RE; break;
        case SIGBUS	: rs = RE; break;
        case SIGILL	: rs = RE; break;
        case SIGKILL: rs = RE; break;
        case SIGTRAP: rs = SLE; break;
        default     : rs = RE; break;
        }
      } else if (case_result.exitcode) {
        rs = RE;
      } else if (case_result.time > time_limit){
        rs = TE;
      } else if (case_result.time > memory_limit){
        rs = ME;
      } else {
        rs = do_compare(j, extra);;
      }
      r["status"] = static_cast<int>(rs);
      r["result"] = getStatusText(rs);
      result["detail"].push_back(r);

      total_time += case_result.time;
      max_memory = max(case_result.memory, max_memory);

      if (!should_continue(j, rs)) {
        fatal_status = rs;
        break;
      }
    }
  }

  result["time"] = total_time;
  result["memory"] = max_memory;

  if (fatal_status != AC)
    finish(fatal_status);
  
  if (total_time > total_time_limit) {
    finish(TE);
  } else if (max_memory > memory_limit) {
    cerr << "should not caught here" << endl;
    finish(ME);
  }

  for (auto& element : result["detail"]) {
    RESULT r = static_cast<RESULT>(element["status"].get<int>());
    if (r != AC)
      finish(r);
  }

  finish(AC);

  cerr << "should not" << endl;
  return -1;
}


int main (int argc, char** argv) {
  json j;
  int r;

  if (r = read_config(argc, argv, j))
    _exit(255);

  if (r = validate_config(j))
    _exit(255);

  string pid = to_string(j["pid"].get<int>());
  string sid = to_string(j["sid"].get<int>());

  path["base"] = j["base_dir"].get<string>();

  path["temp"] = "/tmp/judge-" + pid + "-" + sid + "/";
  delete_file(path["temp"]);
  create_folder(path["temp"]);

  path["output"] = path["base"] + "/result/" + pid + "/" + sid + "/";
  create_folder(path["output"]);

  path["stdin"] = path["base"] + "/case/" + pid +"/";
  path["stdout"] = path["base"] + "/case/" + pid + "/";

  path["diff"] = path["temp"];

  // Files
  path["result"] = path["output"] + "/result.json";

  path["code"] = path["base"] + "/code/" + pid + "/" + sid + "/" + j["filename"].get<string>();
  
  mode_t original_mask = umask(0777);
  path["exec"] = path["temp"] + "/main";
  unlink(path["exec"].c_str());
  ofstream(path["exec"]).close();
  chmod(path["exec"].c_str(), S_IRWXG|S_IRWXU);

  path["spj"] = path["base"] + "/judge/" + pid;
  path["spjlog"] = path["temp"] + "result.spjinfo";
  unlink(path["spjlog"].c_str());

  path["cmpinfo"] = path["output"] + "/result.cmpinfo";
  unlink(path["cmpinfo"].c_str ());

  result["time"] = 0;
  result["memory"] = 0;
  result["result"] = "system error";
  result["status"] = static_cast<int>(SW);
  result["detail"] = {};
  result["compiler"] = nullptr;
  result["extra"] = nullptr;

  // compile source or check syntax
  if(r = generate_exec_args(j))
    _exit(255);
  
  // do test
  if(r = do_test(j))
    _exit(255);

  return 0;
}