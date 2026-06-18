#pragma once
#include "BlockType.h"
#include "MethodAccess.h"
#include "MethodArg.h"
#include "MethodType.h"
#include <string>
#include <vector>

namespace back::model {

  struct Block
  {
    std::string            id;
    std::string            unit_id;          // owning unit (unit.id)
    BlockType              type = BlockType::Method;
    float                  x = 0, y = 0;     // top-left, world coordinates
    float                  width = 0, height = 0;
    std::string            name;             // from unit_b_method / unit_b_field
    std::vector<MethodArg> args;             // method arguments, ordered (methods only)
    // disabled/access come from unit_b_method or unit_b_field per `type`.
    bool                   disabled = false;
    MethodType             method_type = MethodType::Inner; // method-only; ignored for fields
    MethodAccess           access = MethodAccess::Private;
    bool                   expr_id_used = false; // field-only: use the type expression instead of size_bytes
    int                    size_bytes   = 0;     // field-only: explicit size when expr_id_used is false
  };

} // namespace back::model
