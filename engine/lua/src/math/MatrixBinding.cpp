#include "math/MatrixBinding.hpp"

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Table.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

// Takes nothing for the identity, the nine numbers by columns, or one value in any form that a `Mat3` argument accepts.
int MatrixBinding::newMat3(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Stack::push(L, haylen::math::Mat3::identity());
        return 1;
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Mat3>(L, 1));
        return 1;
    }

    haylen::math::Mat3 matrix;
    for (std::size_t place = 0; place < matrix.values.size(); ++place) {
        matrix.values[place] = lua::Stack::read<float>(L, static_cast<int>(place + 1));
    }

    lua::Stack::push(L, matrix);
    return 1;
}

// Takes nothing for the identity, the sixteen numbers by columns, or one value in any form that a `Mat4` argument accepts.
int MatrixBinding::newMat4(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Stack::push(L, haylen::math::Mat4::identity());
        return 1;
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Mat4>(L, 1));
        return 1;
    }

    haylen::math::Mat4 matrix;
    for (std::size_t place = 0; place < matrix.values.size(); ++place) {
        matrix.values[place] = lua::Stack::read<float>(L, static_cast<int>(place + 1));
    }

    lua::Stack::push(L, matrix);
    return 1;
}

haylen::math::Mat4 MatrixBinding::compose(haylen::math::Vec3 translation, std::optional<haylen::math::Quat> rotation, std::optional<haylen::math::Vec3> scale) {
    return haylen::math::Mat4::compose(translation, rotation.value_or(haylen::math::Quat::identity()), scale.value_or(haylen::math::Vec3{1.0F, 1.0F, 1.0F}));
}

haylen::math::Mat4 MatrixBinding::lookAt(haylen::math::Vec3 eye, haylen::math::Vec3 target, std::optional<haylen::math::Vec3> up) {
    return haylen::math::Mat4::lookAt(eye, target, up.value_or(haylen::math::Vec3::up()));
}

int MatrixBinding::perspective(lua_State* L) {
    haylen::math::Mat4::Perspective options;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kPerspectiveFields});
        lua::Table::readField(L, 1, "fieldOfView", options.fieldOfView);
        lua::Table::readField(L, 1, "aspect", options.aspect);
        lua::Table::readField(L, 1, "nearPlane", options.nearPlane);
        lua::Table::readField(L, 1, "farPlane", options.farPlane);
        lua::Table::readField(L, 1, "depthRange", options.depthRange);
        lua::Table::readField(L, 1, "reversedDepth", options.reversedDepth);
    }

    lua::Stack::push(L, haylen::math::Mat4::perspective(options));
    return 1;
}

int MatrixBinding::asymmetricPerspective(lua_State* L) {
    haylen::math::Mat4::AsymmetricPerspective options;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kBoundsFields});
        lua::Table::readField(L, 1, "left", options.left);
        lua::Table::readField(L, 1, "right", options.right);
        lua::Table::readField(L, 1, "bottom", options.bottom);
        lua::Table::readField(L, 1, "top", options.top);
        lua::Table::readField(L, 1, "nearPlane", options.nearPlane);
        lua::Table::readField(L, 1, "farPlane", options.farPlane);
        lua::Table::readField(L, 1, "depthRange", options.depthRange);
        lua::Table::readField(L, 1, "reversedDepth", options.reversedDepth);
    }

    lua::Stack::push(L, haylen::math::Mat4::asymmetricPerspective(options));
    return 1;
}

int MatrixBinding::orthographic(lua_State* L) {
    haylen::math::Mat4::Orthographic options;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kBoundsFields});
        lua::Table::readField(L, 1, "left", options.left);
        lua::Table::readField(L, 1, "right", options.right);
        lua::Table::readField(L, 1, "bottom", options.bottom);
        lua::Table::readField(L, 1, "top", options.top);
        lua::Table::readField(L, 1, "nearPlane", options.nearPlane);
        lua::Table::readField(L, 1, "farPlane", options.farPlane);
        lua::Table::readField(L, 1, "depthRange", options.depthRange);
        lua::Table::readField(L, 1, "reversedDepth", options.reversedDepth);
    }

    lua::Stack::push(L, haylen::math::Mat4::orthographic(options));
    return 1;
}

std::size_t MatrixBinding::checkIndex(lua_State* L, int index, std::size_t count) {
    const lua_Integer value = luaL_checkinteger(L, index);
    if (value < 1 || value > static_cast<lua_Integer>(count)) {
        luaL_error(L, "A row or a column of this matrix is a whole number from 1 to %d, not %d.", static_cast<int>(count), static_cast<int>(value));
    }

    return static_cast<std::size_t>(value - 1);
}

int MatrixBinding::mat3Get(lua_State* L) {
    const haylen::math::Mat3& self = lua::Userdata::check<haylen::math::Mat3>(L, 1);
    const std::size_t row = checkIndex(L, 2, 3);
    const std::size_t column = checkIndex(L, 3, 3);
    lua::Stack::push(L, self.get(row, column));
    return 1;
}

int MatrixBinding::mat3Column(lua_State* L) {
    const haylen::math::Mat3& self = lua::Userdata::check<haylen::math::Mat3>(L, 1);
    lua::Stack::push(L, self.getColumn(checkIndex(L, 2, 3)));
    return 1;
}

int MatrixBinding::mat3Row(lua_State* L) {
    const haylen::math::Mat3& self = lua::Userdata::check<haylen::math::Mat3>(L, 1);
    lua::Stack::push(L, self.getRow(checkIndex(L, 2, 3)));
    return 1;
}

// Multiplies by another `Mat3`, or by a table of nine numbers, and otherwise maps a vector.
int MatrixBinding::mat3Multiply(lua_State* L) {
    const haylen::math::Mat3 self = lua::Stack::read<haylen::math::Mat3>(L, 1);
    const bool matrix = lua::Userdata::test<haylen::math::Mat3>(L, 2) != nullptr || (lua_istable(L, 2) && lua_rawlen(L, 2) == 9);
    if (matrix) {
        lua::Stack::push(L, self * lua::Stack::read<haylen::math::Mat3>(L, 2));
        return 1;
    }

    lua::Stack::push(L, self * lua::Stack::read<haylen::math::Vec3>(L, 2));
    return 1;
}

int MatrixBinding::mat3Unpack(lua_State* L) {
    const haylen::math::Mat3& self = lua::Userdata::check<haylen::math::Mat3>(L, 1);
    for (const float value : self.values) {
        lua::Stack::push(L, value);
    }

    return static_cast<int>(self.values.size());
}

int MatrixBinding::mat3ToString(lua_State* L) {
    const haylen::math::Mat3& self = lua::Userdata::check<haylen::math::Mat3>(L, 1);
    const std::array<float, 9>& v = self.values;
    lua_pushfstring(L, "Mat3((%f, %f, %f), (%f, %f, %f), (%f, %f, %f))", NumberText::plain(v[0]), NumberText::plain(v[1]), NumberText::plain(v[2]), NumberText::plain(v[3]), NumberText::plain(v[4]), NumberText::plain(v[5]), NumberText::plain(v[6]), NumberText::plain(v[7]), NumberText::plain(v[8]));
    return 1;
}

haylen::math::Mat3 MatrixBinding::mat3Inverse(const haylen::math::Mat3& self) {
    return self.getInverse();
}

haylen::math::Quat MatrixBinding::mat3Rotation(const haylen::math::Mat3& self) {
    return haylen::math::Quat::fromMatrix(self);
}

bool MatrixBinding::mat3Approximately(const haylen::math::Mat3& self, const haylen::math::Mat3& other, std::optional<float> epsilon) {
    return haylen::math::Mat3::approximately(self, other, epsilon.value_or(1e-5F));
}

int MatrixBinding::mat4Get(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    const std::size_t row = checkIndex(L, 2, 4);
    const std::size_t column = checkIndex(L, 3, 4);
    lua::Stack::push(L, self.get(row, column));
    return 1;
}

int MatrixBinding::mat4Column(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    lua::Stack::push(L, self.getColumn(checkIndex(L, 2, 4)));
    return 1;
}

int MatrixBinding::mat4Row(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    lua::Stack::push(L, self.getRow(checkIndex(L, 2, 4)));
    return 1;
}

// Multiplies by another `Mat4`, or by a table of sixteen numbers, and otherwise maps a `Vec4`.
int MatrixBinding::mat4Multiply(lua_State* L) {
    const haylen::math::Mat4 self = lua::Stack::read<haylen::math::Mat4>(L, 1);
    const bool matrix = lua::Userdata::test<haylen::math::Mat4>(L, 2) != nullptr || (lua_istable(L, 2) && lua_rawlen(L, 2) == 16);
    if (matrix) {
        lua::Stack::push(L, self * lua::Stack::read<haylen::math::Mat4>(L, 2));
        return 1;
    }

    lua::Stack::push(L, self * lua::Stack::read<haylen::math::Vec4>(L, 2));
    return 1;
}

int MatrixBinding::mat4Unpack(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    for (const float value : self.values) {
        lua::Stack::push(L, value);
    }

    return static_cast<int>(self.values.size());
}

int MatrixBinding::mat4ToString(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    const std::array<float, 16>& v = self.values;
    lua_pushfstring(L, "Mat4((%f, %f, %f, %f), (%f, %f, %f, %f), ", NumberText::plain(v[0]), NumberText::plain(v[1]), NumberText::plain(v[2]), NumberText::plain(v[3]), NumberText::plain(v[4]), NumberText::plain(v[5]), NumberText::plain(v[6]), NumberText::plain(v[7]));
    lua_pushfstring(L, "(%f, %f, %f, %f), (%f, %f, %f, %f))", NumberText::plain(v[8]), NumberText::plain(v[9]), NumberText::plain(v[10]), NumberText::plain(v[11]), NumberText::plain(v[12]), NumberText::plain(v[13]), NumberText::plain(v[14]), NumberText::plain(v[15]));
    lua_concat(L, 2);
    return 1;
}

int MatrixBinding::mat4Decompose(lua_State* L) {
    const haylen::math::Mat4::Decomposition parts = lua::Userdata::check<haylen::math::Mat4>(L, 1).decompose();
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, parts.translation);
    lua_setfield(L, -2, "translation");
    lua::Stack::push(L, parts.rotation);
    lua_setfield(L, -2, "rotation");
    lua::Stack::push(L, parts.scale);
    lua_setfield(L, -2, "scale");
    return 1;
}

std::pair<haylen::math::Mat4::DepthRange, bool> MatrixBinding::readDepthOptions(lua_State* L, int index) {
    haylen::math::Mat4::DepthRange depthRange = haylen::math::Mat4::DepthRange::ZeroToOne;
    bool reversedDepth = true;
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kDepthFields});
        lua::Table::readField(L, index, "depthRange", depthRange);
        lua::Table::readField(L, index, "reversedDepth", reversedDepth);
    }

    return {depthRange, reversedDepth};
}

// Takes the plane in the space of the camera and the options of `readDepthOptions`.
int MatrixBinding::mat4ObliqueNearPlane(lua_State* L) {
    const haylen::math::Mat4& self = lua::Userdata::check<haylen::math::Mat4>(L, 1);
    const haylen::math::Plane plane = lua::Stack::read<haylen::math::Plane>(L, 2);
    const auto [depthRange, reversedDepth] = readDepthOptions(L, 3);
    lua::Stack::push(L, self.getObliqueNearPlane(plane, depthRange, reversedDepth));
    return 1;
}

haylen::math::Mat4 MatrixBinding::mat4Inverse(const haylen::math::Mat4& self) {
    return self.getInverse();
}

haylen::math::Mat4 MatrixBinding::mat4AffineInverse(const haylen::math::Mat4& self) {
    return self.getAffineInverse();
}

haylen::math::Mat3 MatrixBinding::mat4NormalMatrix(const haylen::math::Mat4& self) {
    return self.getNormalMatrix();
}

bool MatrixBinding::mat4Approximately(const haylen::math::Mat4& self, const haylen::math::Mat4& other, std::optional<float> epsilon) {
    return haylen::math::Mat4::approximately(self, other, epsilon.value_or(1e-5F));
}

void MatrixBinding::install(lua_State* L) {
    using haylen::math::Mat3;
    using haylen::math::Mat4;

    lua::ClassBuilder<Mat3>(L).function("get", &lua::Binding::native<&mat3Get>).function("column", &lua::Binding::native<&mat3Column>).function("row", &lua::Binding::native<&mat3Row>).method<&Mat3::getTransposed>("transposed").method<&Mat3::getDeterminant>("determinant").method<&Mat3::isInvertible>("isInvertible").function("inverse", &lua::Binding::function<&mat3Inverse>).function("rotation", &lua::Binding::function<&mat3Rotation>).function("approximately", &lua::Binding::function<&mat3Approximately>).function("unpack", &lua::Binding::native<&mat3Unpack>).meta("__mul", &lua::Binding::native<&mat3Multiply>).meta("__eq", &lua::Binding::native<&equal<Mat3>>).meta("__tostring", &lua::Binding::native<&mat3ToString>).install();

    lua::ClassBuilder<Mat4>(L).function("get", &lua::Binding::native<&mat4Get>).function("column", &lua::Binding::native<&mat4Column>).function("row", &lua::Binding::native<&mat4Row>).method<&Mat4::getTranslation>("translation").method<&Mat4::getLinear>("linear").method<&Mat4::apply>("apply").method<&Mat4::applyVector>("applyVector").method<&Mat4::getTransposed>("transposed").method<&Mat4::getDeterminant>("determinant").method<&Mat4::isInvertible>("isInvertible").method<&Mat4::isAffine>("isAffine").function("inverse", &lua::Binding::function<&mat4Inverse>).function("affineInverse", &lua::Binding::function<&mat4AffineInverse>).function("normalMatrix", &lua::Binding::function<&mat4NormalMatrix>).function("decompose", &lua::Binding::native<&mat4Decompose>).function("obliqueNearPlane", &lua::Binding::native<&mat4ObliqueNearPlane>).function("approximately", &lua::Binding::function<&mat4Approximately>).function("unpack", &lua::Binding::native<&mat4Unpack>).meta("__mul", &lua::Binding::native<&mat4Multiply>).meta("__eq", &lua::Binding::native<&equal<Mat4>>).meta("__tostring", &lua::Binding::native<&mat4ToString>).install();
}

void MatrixBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"mat3", &lua::Binding::native<&newMat3>}, {"mat4", &lua::Binding::native<&newMat4>}, {"compose", &lua::Binding::function<&compose>}, {"lookAt", &lua::Binding::function<&lookAt>}, {"perspective", &lua::Binding::native<&perspective>}, {"asymmetricPerspective", &lua::Binding::native<&asymmetricPerspective>}, {"orthographic", &lua::Binding::native<&orthographic>}, {"depthRangeConversion", &lua::Binding::function<&haylen::math::Mat4::depthRangeConversion>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
