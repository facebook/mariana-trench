/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/local_flow/LocalFlowLabel.h>

#include <algorithm>

#include <fmt/format.h>

namespace marianatrench {
namespace local_flow {

Label::Label(const StructuralLabel& sl)
    : kind([&] {
        switch (sl.op) {
          case StructuralOp::Project:
            return LabelKind::Project;
          case StructuralOp::Inject:
            return LabelKind::Inject;
          case StructuralOp::ContraProject:
            return LabelKind::ContraProject;
          case StructuralOp::ContraInject:
            return LabelKind::ContraInject;
        }
        return LabelKind::Project; // unreachable
      }()),
      value(sl.field) {}

bool Label::operator==(const Label& other) const {
  return kind == other.kind && value == other.value &&
      preserves_context == other.preserves_context &&
      call_site == other.call_site;
}

bool Label::operator!=(const Label& other) const {
  return !(*this == other);
}

bool Label::operator<(const Label& other) const {
  if (kind != other.kind) {
    return kind < other.kind;
  }
  if (value != other.value) {
    return value < other.value;
  }
  if (preserves_context != other.preserves_context) {
    return preserves_context < other.preserves_context;
  }
  return call_site < other.call_site;
}

bool Label::is_structural() const {
  switch (kind) {
    case LabelKind::Project:
    case LabelKind::Inject:
    case LabelKind::ContraProject:
    case LabelKind::ContraInject:
    case LabelKind::Interval:
      return true;
    default:
      return false;
  }
}

bool Label::is_call() const {
  switch (kind) {
    case LabelKind::Call:
    case LabelKind::Return:
    case LabelKind::HoCall:
    case LabelKind::HoReturn:
    case LabelKind::Capture:
    case LabelKind::HoCapture:
      return true;
    default:
      return false;
  }
}

bool Label::is_call_boundary() const {
  return kind == LabelKind::Call || kind == LabelKind::HoCall ||
      kind == LabelKind::Capture;
}

Label Label::inverse() const {
  switch (kind) {
    case LabelKind::Project:
      return Label{LabelKind::Inject, value};
    case LabelKind::Inject:
      return Label{LabelKind::Project, value};
    case LabelKind::ContraProject:
      return Label{LabelKind::ContraInject, value};
    case LabelKind::ContraInject:
      return Label{LabelKind::ContraProject, value};
    case LabelKind::Call:
      return Label{LabelKind::Return, {}, preserves_context, call_site};
    case LabelKind::Return:
      return Label{LabelKind::Call, {}, preserves_context, call_site};
    case LabelKind::HoCall:
      return Label{LabelKind::HoReturn, {}, preserves_context, call_site};
    case LabelKind::HoReturn:
      return Label{LabelKind::HoCall, {}, preserves_context, call_site};
    case LabelKind::Capture:
      return *this;
    case LabelKind::HoCapture:
      return *this;
    case LabelKind::Interval:
      return *this;
  }

  mt_unreachable();
}

std::string Label::to_string() const {
  switch (kind) {
    case LabelKind::Project:
    case LabelKind::Inject:
    case LabelKind::ContraProject:
    case LabelKind::ContraInject: {
      std::string field_str = value.empty() ? "*" : value;
      const char* kind_str = nullptr;
      switch (kind) {
        case LabelKind::Project:
          kind_str = "Project";
          break;
        case LabelKind::Inject:
          kind_str = "Inject";
          break;
        case LabelKind::ContraProject:
          kind_str = "ContraProject";
          break;
        case LabelKind::ContraInject:
          kind_str = "ContraInject";
          break;
        default:
          break;
      }
      return fmt::format("{}:{}", kind_str, field_str);
    }
    case LabelKind::Call:
    case LabelKind::Return:
    case LabelKind::HoCall:
    case LabelKind::HoReturn:
    case LabelKind::Capture:
    case LabelKind::HoCapture: {
      const char* kind_str = nullptr;
      switch (kind) {
        case LabelKind::Call:
          kind_str = "Call";
          break;
        case LabelKind::Return:
          kind_str = "Return";
          break;
        case LabelKind::HoCall:
          kind_str = "HoCall";
          break;
        case LabelKind::HoReturn:
          kind_str = "HoReturn";
          break;
        case LabelKind::Capture:
          kind_str = "Capture";
          break;
        case LabelKind::HoCapture:
          kind_str = "HoCapture";
          break;
        default:
          break;
      }
      std::string result = kind_str;
      if (preserves_context) {
        result += "+";
      }
      if (!call_site.empty()) {
        result += ":";
        result += call_site;
      }
      return result;
    }
    case LabelKind::Interval:
      return fmt::format("Interval:{}", value);
  }

  mt_unreachable();
}

Json::Value Label::to_json() const {
  return Json::Value(to_string());
}

bool is_contra_path(const LabelPath& labels) {
  int contra_count =
      std::count_if(labels.begin(), labels.end(), [](const Label& l) {
        return l.kind == LabelKind::ContraProject ||
            l.kind == LabelKind::ContraInject;
      });
  return contra_count % 2 == 1;
}

Guard reverse_guard(Guard guard, const LabelPath& labels) {
  return is_contra_path(labels) ? guard : flip_guard(guard);
}

bool StructuralLabel::operator==(const StructuralLabel& other) const {
  return op == other.op && field == other.field;
}

bool StructuralLabel::operator!=(const StructuralLabel& other) const {
  return !(*this == other);
}

bool StructuralLabel::operator<(const StructuralLabel& other) const {
  if (op != other.op) {
    return op < other.op;
  }
  return field < other.field;
}

StructuralLabel StructuralLabel::dual() const {
  switch (op) {
    case StructuralOp::Project:
      return StructuralLabel{StructuralOp::ContraProject, field};
    case StructuralOp::Inject:
      return StructuralLabel{StructuralOp::ContraInject, field};
    case StructuralOp::ContraProject:
      return StructuralLabel{StructuralOp::Project, field};
    case StructuralOp::ContraInject:
      return StructuralLabel{StructuralOp::Inject, field};
  }

  mt_unreachable();
}

StructuralLabel StructuralLabel::inverse() const {
  switch (op) {
    case StructuralOp::Project:
      return StructuralLabel{StructuralOp::Inject, field};
    case StructuralOp::Inject:
      return StructuralLabel{StructuralOp::Project, field};
    case StructuralOp::ContraProject:
      return StructuralLabel{StructuralOp::ContraInject, field};
    case StructuralOp::ContraInject:
      return StructuralLabel{StructuralOp::ContraProject, field};
  }

  mt_unreachable();
}

std::string structural_op_to_string(StructuralOp op) {
  switch (op) {
    case StructuralOp::Project:
      return "Project";
    case StructuralOp::Inject:
      return "Inject";
    case StructuralOp::ContraProject:
      return "ContraProject";
    case StructuralOp::ContraInject:
      return "ContraInject";
  }

  mt_unreachable();
}

std::string StructuralLabel::to_string() const {
  std::string field_str = field.empty() ? "*" : field;
  return fmt::format("{}:{}", structural_op_to_string(op), field_str);
}

Json::Value StructuralLabel::to_json() const {
  return Json::Value(to_string());
}

} // namespace local_flow
} // namespace marianatrench
