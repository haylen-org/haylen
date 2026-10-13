#include "haylen/2d/graphics/PostProcess.hpp"

namespace haylen::graphics2d {

// The simulations are the matrices of full severity that Machado, Oliveira and Fernandes derived from the physiology of the cones. The correction adds the difference between a color and its simulation back into the channels the deficiency still sees, which keeps the whole operation one matrix.
std::array<std::array<float, 3>, 3> PostProcess::getColorVisionMatrix() const noexcept {
    using Matrix = std::array<std::array<float, 3>, 3>;
    constexpr Matrix kIdentity{{{1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}}};
    constexpr Matrix kProtanopia{{{0.152286F, 1.052583F, -0.204868F}, {0.114503F, 0.786281F, 0.099216F}, {-0.003882F, -0.048116F, 1.051998F}}};
    constexpr Matrix kDeuteranopia{{{0.367322F, 0.860646F, -0.227968F}, {0.280085F, 0.672501F, 0.047413F}, {-0.011820F, 0.042940F, 0.968881F}}};
    constexpr Matrix kTritanopia{{{1.255528F, -0.076749F, -0.178779F}, {-0.078411F, 0.930809F, 0.147602F}, {0.004733F, 0.691367F, 0.303900F}}};
    constexpr Matrix kShift{{{0.0F, 0.0F, 0.0F}, {0.7F, 1.0F, 0.0F}, {0.7F, 0.0F, 1.0F}}};

    const Matrix* simulation = nullptr;
    switch (colorVision) {
    case ColorVision::Normal:
        return kIdentity;
    case ColorVision::Protanopia:
        simulation = &kProtanopia;
        break;
    case ColorVision::Deuteranopia:
        simulation = &kDeuteranopia;
        break;
    case ColorVision::Tritanopia:
        simulation = &kTritanopia;
        break;
    }
    if (!correctColorVision) {
        return *simulation;
    }

    // The corrected color is c + shift * (c - simulation * c), so the matrix is identity + shift * (identity - simulation).
    Matrix corrected = kIdentity;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t inner = 0; inner < 3; ++inner) {
                corrected[row][column] += kShift[row][inner] * (kIdentity[inner][column] - (*simulation)[inner][column]);
            }
        }
    }
    return corrected;
}

} // namespace haylen::graphics2d
