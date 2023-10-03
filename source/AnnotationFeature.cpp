#include <mariana-trench/AnnotationFeature.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

AnnotationFeature::AnnotationFeature(Root port, const DexType* dex_type, std::optional<std::string> label) : port_(std::move(port)), dex_type_(dex_type), label_(label) {
}

const AnnotationFeature* AnnotationFeature::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);
  Root port = Root::from_json(value["port"]);
  const DexType* dex_type = JsonValidation::dex_type(value, "type");
  Json::Value configured_label = value["label"];
  std::optional<std::string> label;
  if (!configured_label.isNull()) {
    label = JsonValidation::string(configured_label);
  }
  return context.feature_factory->get_unique_annotation_feature(AnnotationFeature{std::move(port), dex_type, std::move(label)});
}

std::size_t AnnotationFeature::hash() const {
  std::size_t seed = 0;
  boost::hash_combine(seed, std::hash<Root>{}(port_));
  boost::hash_combine(seed, dex_type_);
  boost::hash_combine(seed, label_);
  return seed;
}

bool AnnotationFeature::operator==(const AnnotationFeature& other) const {
  return port_ == other.port_ && dex_type_ == other.dex_type_ && label_ == other.label_;
}

const Root& AnnotationFeature::port() const {
  return port_;
}

const DexType* AnnotationFeature::dex_type() const {
  return dex_type_;
}

const std::optional<std::string>& AnnotationFeature::label() const {
  return label_;
}

std::ostream& operator<<(std::ostream& out, const AnnotationFeature& annotation_feature) {
  out << "AnnotationFeature(port=`"
      << annotation_feature.port()
      << "`, dex_type=`"
      << annotation_feature.dex_type()->str();
  if (annotation_feature.label()) {
    out << "`, label=`" << *annotation_feature.label();
  }
  return out << "`)";
}

}
