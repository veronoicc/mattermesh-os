#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "../services/selftest.h"
#include "selftest_layout.h"
namespace mm {
// One row of the check list: status is a selftest::Status. No default member initializers: the firmware builds as C++11 and the rows are brace-initialised.
struct SelfTestRow {std::string label,detail;int status;uint32_t ms;};
// Self-test app: everything the page draws; filled by selfTestCompose.
struct SelfTestView {
 std::vector<SelfTestRow> rows;
 int pass=0,fail=0,skip=0,notRun=0,first=0,focus=0,button=-1,optionFocus=0;// focus: row (button -1) or button 0..2 (row = the one RUN ONE runs)
 bool busy=false,options=false,interactive=true,output=true;
 int prompt=0,promptSecs=0;// 0 none, 1 press a key, 2 tap the target (the running check waits for the user)
 std::string line,toast,promptTitle,promptText;// line: progress under the totals
};
}
