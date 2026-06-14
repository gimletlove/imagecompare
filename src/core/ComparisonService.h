#pragma once

#include "core/ComparisonResult.h"

class ImageRepository;

[[nodiscard]] ComparisonResult run_comparison(ImageRepository& repository, const ComparisonRequest& request);
