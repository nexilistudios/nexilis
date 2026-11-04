/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#ifndef NEXILIS_SERVER_VALIDATION_HH
#define NEXILIS_SERVER_VALIDATION_HH

#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/types/vector2.hh>
#include <nexilis/types/vector3.hh>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace nexilis::server::validation
{

/// Check that a label contains only letters, digits, underscores, and dashes.
inline bool isValidLabel(std::string_view value, size_t maxLength)
{
    return !value.empty() && value.size() <= maxLength &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch)
                       { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'; });
}

/// Check that a path is relative and consists of safe path segments.
inline bool isValidAssetPath(std::string_view path)
{
    if (path.empty() || path.size() > 256 || path.front() == '/')
        return false;
    size_t start = 0;
    while (start < path.size())
    {
        const size_t end = path.find('/', start);
        const auto segment = path.substr(start, end == std::string_view::npos ? end : end - start);
        if (segment.empty() || segment == "." || segment == ".." ||
            !std::all_of(segment.begin(), segment.end(), [](unsigned char ch)
                         { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'; }))
            return false;
        if (end == std::string_view::npos)
            return true;
        start = end + 1;
    }
    return false;
}

/// An empty asset path means "no asset", which is valid for room objects.
inline bool isValidOptionalAssetPath(std::string_view path)
{
    return path.empty() || isValidAssetPath(path);
}

inline bool isFinite(const Vector2f& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

inline bool isFinite(const Vector3f& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline bool isValidPosition(const Vector2f& position)
{
    const auto& limits = GameplayLimits::get();
    return isFinite(position) &&
           std::abs(position.x) <= limits.getMaxHorizontalPosition() &&
           std::abs(position.y) <= limits.getMaxHorizontalPosition();
}

inline bool isValidPosition(const Vector3f& position)
{
    const auto& limits = GameplayLimits::get();
    return isFinite(position) &&
           std::abs(position.x) <= limits.getMaxHorizontalPosition() &&
           std::abs(position.z) <= limits.getMaxHorizontalPosition() &&
           position.y >= limits.getMinHeight() && position.y <= limits.getMaxHeight();
}

inline bool isValidDimensions(const Vector2f& dimensions)
{
    const auto& limits = GameplayLimits::get();
    const float maxDimension = limits.getMaxHorizontalPosition();
    return isFinite(dimensions) && dimensions.x > 0.0f && dimensions.y > 0.0f &&
           dimensions.x <= maxDimension && dimensions.y <= maxDimension;
}

inline bool isValidDimensions(const Vector3f& dimensions)
{
    const auto& limits = GameplayLimits::get();
    const float maxDimension = limits.getMaxHorizontalPosition();
    return isFinite(dimensions) && dimensions.x > 0.0f && dimensions.y > 0.0f &&
           dimensions.z > 0.0f && dimensions.x <= maxDimension &&
           dimensions.y <= maxDimension && dimensions.z <= maxDimension;
}

} // namespace nexilis::server::validation

#endif
