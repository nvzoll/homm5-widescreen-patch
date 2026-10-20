#pragma once

struct VirtualScreen {
    float width;
    float height;
};

// ToE 3.1's rule: keep the design height on wider screens, the design width on narrower ones.
inline VirtualScreen ToeVirtualScreen(int width, int height, float designWidth, float designHeight) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    VirtualScreen result{designWidth, designHeight};
    if (aspect > 4.0f / 3.0f) {
        result.width = static_cast<float>(static_cast<int>(designHeight * aspect + 0.99f));
    } else {
        result.height = static_cast<float>(static_cast<int>(designWidth / aspect + 0.99f));
    }
    return result;
}
