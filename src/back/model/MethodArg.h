#pragma once
#include <string>

namespace back::model {

  // One argument of a method block (a row of unit_b_method_arg). Drawn as a
  // row below the method's badge; absent for field blocks.
  struct MethodArg
  {
    std::string id;
    std::string name;
    double      order_index = 0; // sort key within the owning method
  };

} // namespace back::model
