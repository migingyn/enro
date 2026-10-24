// Exponentially weighted moving average, used by the punctuality model (PRD 7.8).
#pragma once

namespace enro {

class Ewma {
 public:
  explicit Ewma(double alpha = 0.3, double initial = 0.0) : alpha_(alpha), value_(initial) {}

  // First sample sets the value; later samples blend in with weight alpha.
  void add(double sample) {
    value_ = samples_ == 0 ? sample : alpha_ * sample + (1.0 - alpha_) * value_;
    ++samples_;
  }

  double value() const { return value_; }
  int samples() const { return samples_; }

 private:
  double alpha_;
  double value_;
  int samples_ = 0;
};

}  // namespace enro
