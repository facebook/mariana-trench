/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <DexAnnotation.h>
#include <fmt/format.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/AnnotationFeatureTemplate.h>

namespace marianatrench {

AnnotationTarget::AnnotationTarget(Kind kind,
                                   std::optional<RootTemplate> argument)
    : kind_(kind), argument_(std::move(argument)) {}

AnnotationTarget AnnotationTarget::from_json(const Json::Value& value) {
  const std::string target = JsonValidation::string(value);
  Kind kind;
  std::optional<RootTemplate> argument;
  if ("Class" == target) {
    kind = Kind::Class;
  } else if ("Method" == target) {
    kind = Kind::Method;
  } else {
    kind = Kind::Parameter;
    try {
      argument = RootTemplate::from_json(value);
    } catch (...) {
      std::throw_with_nested(JsonValidationError(
          value,
          /* field */ std::nullopt,
          fmt::format("valid annotation target (`Class`, `Method`, or "
                      "`Argument(...)`), got `{}`",
                      target)));
    }
  }
  return AnnotationTarget{kind, std::move(argument)};
}

AnnotationTarget::Kind AnnotationTarget::kind() const { return kind_; }

const RootTemplate& AnnotationTarget::argument() const { return *argument_; }

AnnotationFeatureTemplate::AnnotationFeatureTemplate(
    AnnotationTarget target, const DexType* dex_type,
    const DexString* MT_NULLABLE tag,
    const DexString* MT_NULLABLE annotation_parameter_name)
    : target_(std::move(target)),
      dex_type_(dex_type),
      tag_(tag),
      annotation_parameter_name_(annotation_parameter_name
                                     ? annotation_parameter_name
                                     : DexString::get_string("value")) {}

AnnotationFeatureTemplate AnnotationFeatureTemplate::from_json(
    const Json::Value& value) {
  JsonValidation::check_unexpected_members(
      value, {"target", "type", "tag", "annotation_parameter"});
  AnnotationTarget port = AnnotationTarget::from_json(value["target"]);
  const DexType* dex_type = JsonValidation::dex_type(value, "type");
  Json::Value optional_tag = value["tag"];
  const DexString* tag = nullptr;
  if (!optional_tag.isNull()) {
    tag = DexString::make_string(JsonValidation::string(optional_tag));
  }
  const Json::Value optional_annotation_parameter_name =
      value["annotation_parameter"];
  const DexString* annotation_parameter_name = nullptr;
  if (!optional_annotation_parameter_name.isNull()) {
    annotation_parameter_name = DexString::make_string(
        JsonValidation::string(optional_annotation_parameter_name));
  }

  return AnnotationFeatureTemplate{std::move(port), dex_type, tag,
                                   annotation_parameter_name};
}

const Feature* MT_NULLABLE AnnotationFeatureTemplate::instantiate(
    const Method* method, Context& context,
    const TemplateVariableMapping& parameter_positions) const {
  if (!annotation_parameter_name_) {
    return nullptr;
  }

  const DexAnnotationSet* anno_set = nullptr;
  switch (target_.kind()) {
    case AnnotationTarget::Kind::Class: {
      if (const DexClass* cls = type_class(method->get_class())) {
        anno_set = cls->get_anno_set();
      }
      break;
    }
    case AnnotationTarget::Kind::Method:
      anno_set = method->dex_method()->get_anno_set();
      break;
    case AnnotationTarget::Kind::Parameter:
      const Root root = target_.argument().instantiate(parameter_positions);
      anno_set = method->get_parameter_annotations(root.parameter_position());
      if (!anno_set) {
        return nullptr;
      }
      break;
  }

  const std::vector<std::unique_ptr<DexAnnotation>>& annotations =
      anno_set->get_annotations();
  auto annotation =
      std::find_if(annotations.begin(), annotations.end(),
                   [this](const std::unique_ptr<DexAnnotation>& annotation) {
                     return dex_type_ == annotation->type();
                   });
  if (annotation == annotations.end()) {
    return nullptr;
  }

  const EncodedAnnotations& annotation_elements = (*annotation)->anno_elems();
  auto value = std::find_if(
      annotation_elements.begin(), annotation_elements.end(),
      [this](const DexAnnotationElement& element) {
        return annotation_parameter_name_ == element.string;
      });
  if (value == annotation_elements.end()) {
    return nullptr;
  }

  const std::string user_feature_data = value->encoded_value->show();
  return context.feature_factory->get_via_annotation_feature(user_feature_data,
                                                             tag_);
}

}  // namespace marianatrench
