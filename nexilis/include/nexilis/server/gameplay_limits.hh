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

#ifndef NEXILIS_SERVER_GAMEPLAY_LIMITS_HH
#define NEXILIS_SERVER_GAMEPLAY_LIMITS_HH

#include <nexilis/nexilis_constants.hh>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace nexilis::server
{

/// Gameplay limits the server enforces on player actions. Every limit
/// has a default and is configurable through the server's ServerConfig.
/// Values outside their limits are clamped to the nearest allowed
/// value; shots that arrive faster than the shooting rate allow are
/// dropped.
class GameplayLimits
{
public:
    /// Maximum absolute horizontal (X and Z) coordinate.
    void setMaxHorizontalPosition(float maxHorizontalPosition)
    {
        m_maxHorizontalPosition = maxHorizontalPosition;
    }
    float getMaxHorizontalPosition() const
    {
        return m_maxHorizontalPosition;
    }

    /// Maximum movement speed in units per second.
    void setMaxMovementSpeed(float maxMovementSpeed)
    {
        m_maxMovementSpeed = maxMovementSpeed;
    }
    float getMaxMovementSpeed() const
    {
        return m_maxMovementSpeed;
    }

    /// Maximum shooting rate in shots per second. A non-positive value
    /// disables the rate limit.
    void setMaxShootingRate(float maxShootingRate)
    {
        m_maxShootingRate = maxShootingRate;
    }
    float getMaxShootingRate() const
    {
        return m_maxShootingRate;
    }

    /// Maximum damage a single shot may deal.
    void setMaxDamagePerShot(float maxDamagePerShot)
    {
        m_maxDamagePerShot = maxDamagePerShot;
    }
    float getMaxDamagePerShot() const
    {
        return m_maxDamagePerShot;
    }

    /// Minimum height (Y coordinate).
    void setMinHeight(float minHeight)
    {
        m_minHeight = minHeight;
    }
    float getMinHeight() const
    {
        return m_minHeight;
    }

    /// Maximum height (Y coordinate).
    void setMaxHeight(float maxHeight)
    {
        m_maxHeight = maxHeight;
    }
    float getMaxHeight() const
    {
        return m_maxHeight;
    }

    /// Clamp a horizontal coordinate into [-max, max].
    float clampHorizontal(float value) const
    {
        return std::clamp(value, -m_maxHorizontalPosition, m_maxHorizontalPosition);
    }

    /// Clamp a height into [min, max].
    float clampHeight(float value) const
    {
        return std::clamp(value, m_minHeight, m_maxHeight);
    }

    /// Clamp a movement speed into [0, max].
    float clampSpeed(float value) const
    {
        return std::clamp(value, 0.0f, m_maxMovementSpeed);
    }

    /// Clamp damage into the per-shot maximum.
    float clampDamage(float value) const
    {
        return std::clamp(value, 0.0f, m_maxDamagePerShot);
    }

    /// Normalize a yaw angle (radians) into [-pi, pi]. Non-finite input
    /// normalizes to zero so callers cannot propagate NaN or infinity.
    static float normalizeYaw(float yaw)
    {
        if (!std::isfinite(yaw))
        {
            return 0.0f;
        }

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 2.0f * kPi;

        // Values already inside the range pass through unchanged so their
        // exact bit pattern is preserved instead of picking up rounding from
        // the wrap arithmetic below.
        if (yaw >= -kPi && yaw <= kPi)
        {
            return yaw;
        }

        float wrapped = std::fmod(yaw + kPi, kTwoPi);
        if (wrapped < 0.0f)
        {
            wrapped += kTwoPi;
        }
        return wrapped - kPi;
    }

    /// Minimum interval between two accepted shots. Zero when the
    /// shooting rate limit is disabled.
    std::chrono::duration<float> shotInterval() const
    {
        if (m_maxShootingRate <= 0.0f)
        {
            return std::chrono::duration<float>::zero();
        }
        return std::chrono::duration<float>(1.0f / m_maxShootingRate);
    }

    /// Whether the limits the static command handlers enforce have been
    /// bound to a live configuration.
    static bool isInitialized()
    {
        return m_limits != nullptr;
    }

    /// Bind the limits the static command handlers enforce. Mirrors
    /// Movement::_initialize: the pointer is refreshed on every
    /// dispatch so a fresh Command does not leave handlers reading a
    /// destroyed configuration's limits.
    static void _initialize(const GameplayLimits& limits)
    {
        m_limits = &limits;
    }

    /// The limits the static command handlers enforce, or the defaults
    /// when no configuration has been bound yet.
    static const GameplayLimits& get()
    {
        static const GameplayLimits defaults;
        return m_limits ? *m_limits : defaults;
    }

private:
    float m_maxHorizontalPosition = NEXILIS_MAX_POSITION;
    float m_maxMovementSpeed = 100.0f;
    float m_maxShootingRate = 10.0f;
    float m_maxDamagePerShot = 35.0f;
    float m_minHeight = 0.0f;
    float m_maxHeight = 1000.0f;

    static inline const GameplayLimits* m_limits = nullptr;
};

} // namespace nexilis::server

#endif
