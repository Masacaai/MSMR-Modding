#pragma once
// Rooftop perches at Ryker's for snipers and rocket launchers (recorded in game 2026-11-08 by standing on each one: x, y, z).
// From wave 5 (Campaign) / level 3 (Horde), most of those roles spawn here instead of on the ground; several can share a perch.
namespace rykers {
    struct Perch { float x, y, z; };
    constexpr Perch kPerches[] = {
        { 1239.02f, 12.55f, -752.09f },
        { 1266.03f, 29.98f, -741.76f },
        { 1252.58f, 26.84f, -773.00f },
        { 1256.59f, 28.84f, -788.16f },
        { 1244.20f, 23.42f, -822.58f },
        { 1252.28f, 19.83f, -877.08f },
        { 1284.54f, 9.10f, -870.60f },
        { 1311.61f, 13.83f, -854.46f },
        { 1308.25f, 21.66f, -839.50f },
        { 1281.74f, 21.57f, -821.49f },
        { 1306.19f, 13.83f, -790.23f },
        { 1328.45f, 9.86f, -752.29f },
        { 1288.04f, 8.73f, -743.91f },
        { 1296.91f, 28.80f, -741.35f }
    };
}
