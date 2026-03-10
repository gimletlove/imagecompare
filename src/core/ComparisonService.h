#pragma once

#include "core/ComparisonResult.h"

class ImageRepository;

class ComparisonService {
   public:
    explicit ComparisonService(ImageRepository& repository);
    [[nodiscard]] ComparisonResult run(const ComparisonRequest& request) const;

   private:
    ImageRepository& m_repository;
};
