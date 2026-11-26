#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "support/CountingPackage.hpp"
#include "support/GltfFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::gltf {

class ReaderTest : public ::testing::Test {
  protected:
    [[nodiscard]] static const io::Package& package() {
        static const std::unique_ptr<io::Package> files = io::Package::openDirectory(test::GltfFiles::getFolder());
        return *files;
    }

    [[nodiscard]] static Document read(std::string_view path) {
        return Reader::read(package(), path);
    }

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view path) {
        return package().readAsset(path);
    }
};

// A GLB file reads its buffer from its binary chunk, its image from a buffer view, and every object of the core with the defaults of the specification.
TEST_F(ReaderTest, ReadsAGlbFileWithItsBinaryChunkAndImage) {
    const Document document = read("cube.glb");
    EXPECT_EQ(document.file, "cube.glb");
    EXPECT_EQ(document.asset.version, "2.0");
    EXPECT_EQ(document.asset.copyright, "Made for the tests of Haylen.");
    ASSERT_EQ(document.buffers.size(), 1U);
    EXPECT_EQ(document.buffers[0].storage, Buffer::Storage::Glb);
    EXPECT_EQ(document.buffers[0].data.size(), document.buffers[0].byteLength);
    EXPECT_EQ(document.scene, 0U);
    ASSERT_EQ(document.nodes.size(), 1U);
    EXPECT_EQ(document.nodes[0].name, "Cube");
    EXPECT_EQ(document.nodes[0].extras.at("spin"), true);
    ASSERT_EQ(document.meshes.size(), 1U);
    const Mesh::Primitive& primitive = document.meshes[0].primitives.at(0);
    EXPECT_EQ(primitive.mode, Mesh::Primitive::Mode::Triangles);
    EXPECT_TRUE(primitive.findAttribute("TANGENT").has_value());
    EXPECT_FALSE(primitive.findAttribute("COLOR_0").has_value());
    ASSERT_TRUE(primitive.indices.has_value());
    EXPECT_EQ(document.accessors[*primitive.indices].count, 36U);

    const Material& material = document.materials.at(0);
    EXPECT_EQ(material.name, "Checker");
    EXPECT_EQ(material.metallicFactor, 0.0F);
    EXPECT_EQ(material.roughnessFactor, 0.5F);
    ASSERT_TRUE(material.baseColorTexture.has_value());
    EXPECT_EQ(material.alphaMode, Material::AlphaMode::Opaque);
    EXPECT_EQ(material.alphaCutoff, 0.5F);
    const Sampler& sampler = document.samplers.at(0);
    EXPECT_EQ(sampler.magFilter, Sampler::Filter::Linear);
    EXPECT_EQ(sampler.minFilter, Sampler::Filter::LinearMipmapLinear);
    EXPECT_EQ(sampler.wrapS, Sampler::Wrap::Repeat);

    EXPECT_EQ(document.images.at(0).mimeType, "image/png");
    const graphics::Image image = document.decodeImage(0, 64);
    EXPECT_EQ(image.getWidth(), 16);
    EXPECT_EQ(image.getPixel(0, 0).toRgba8(), math::Color::fromRgba8(230, 120, 40, 255).toRgba8());
}

// A `.gltf` file reads its buffer and its image from files beside it, which it names by relative URIs whose percent-encoded characters decode, and the bytes of the three forms of one model are the same.
TEST_F(ReaderTest, ReadsTheSameModelFromFilesBesideItAndFromDataUris) {
    const Document glb = read("cube.glb");
    Reader reader = Reader::open(package(), "cube/cube.gltf");
    EXPECT_EQ(reader.getExternalFiles(), (std::vector<std::string>{"cube/cube.bin", "textures/checker-board.png"}));
    reader.loadResources();
    const Document separate = reader.finish();
    const Document embedded = read("cube-embedded.gltf");

    EXPECT_EQ(separate.buffers.at(0).storage, Buffer::Storage::File);
    EXPECT_EQ(separate.buffers.at(0).uri, "cube.bin");
    EXPECT_EQ(separate.images.at(0).path, "textures/checker-board.png");
    EXPECT_EQ(separate.images.at(0).mimeType, "image/png");
    EXPECT_EQ(embedded.buffers.at(0).storage, Buffer::Storage::DataUri);
    EXPECT_EQ(embedded.images.at(0).storage, Buffer::Storage::DataUri);
    for (const Document* document : {&separate, &embedded}) {
        EXPECT_EQ(AccessorReader::readFloats(*document, 0), AccessorReader::readFloats(glb, 0));
        EXPECT_EQ(AccessorReader::readUnsigned(*document, 4), AccessorReader::readUnsigned(glb, 4));
        EXPECT_EQ(document->decodeImage(0, 64).getPixels().size(), glb.decodeImage(0, 64).getPixels().size());
    }
}

// The binary chunk of a GLB file in a package streams in by range, so the reader reads the header, the JSON and the buffer and never the whole file twice.
TEST_F(ReaderTest, ReadsTheBinaryChunkOfAGlbFileByRange) {
    auto counting = std::make_shared<test::CountingPackage>(std::shared_ptr<const io::Package>(io::Package::openDirectory(test::GltfFiles::getFolder())));
    const Document document = Reader::read(*counting, "cube.glb");
    EXPECT_LE(counting->getReadBytes(), package().getAssetSize("cube.glb") + 4);
    EXPECT_EQ(counting->getOpenReaders(), 0U);
    EXPECT_EQ(document.buffers.at(0).data.size(), document.buffers.at(0).byteLength);
}

// Bytes in memory read a GLB file and a `.gltf` file whose resources are `data:` URIs.
TEST_F(ReaderTest, ReadsBytesInMemory) {
    const std::vector<std::uint8_t> glb = bytes("cube.glb");
    EXPECT_EQ(Reader::read(glb, "download.glb").meshes.size(), 1U);
    const std::vector<std::uint8_t> text = bytes("cube-embedded.gltf");
    EXPECT_EQ(Reader::read(text, "download.gltf").images.size(), 1U);
}

// Cylinders keep indices of 32 bits and a material with every texture of the metallic-roughness model, its factors, the scale of its normals and the strength of its occlusion.
TEST_F(ReaderTest, ReadsEveryTextureOfTheMaterialModel) {
    const Document document = read("cylinder.glb");
    const Material& material = document.materials.at(0);
    EXPECT_EQ(material.normalScale, 0.75F);
    EXPECT_EQ(material.occlusionStrength, 0.5F);
    ASSERT_TRUE(material.emissiveTexture.has_value());
    ASSERT_TRUE(material.metallicRoughnessTexture.has_value());
    EXPECT_EQ(material.emissiveFactor.z, 1.0F);
    EXPECT_FLOAT_EQ(material.baseColorFactor.z, 0.85F);
    EXPECT_EQ(document.accessors.at(*document.meshes[0].primitives[0].indices).componentType, Accessor::ComponentType::UnsignedInt);
}

// A primitive without indices and without a material draws its vertices in order with the default material, and colors of normalized bytes read as floats from 0 to 1.
TEST_F(ReaderTest, ReadsAPrimitiveWithoutIndicesOrMaterial) {
    const Document document = read("icosphere.glb");
    const Mesh::Primitive& primitive = document.meshes.at(0).primitives.at(0);
    EXPECT_FALSE(primitive.indices.has_value());
    EXPECT_FALSE(primitive.material.has_value());
    EXPECT_EQ(document.accessors.at(primitive.attributes[0].accessor).count, 240U);
    const Material defaults;
    EXPECT_EQ(defaults.baseColorFactor.w, 1.0F);
    EXPECT_EQ(defaults.metallicFactor, 1.0F);
    EXPECT_FALSE(defaults.doubleSided);

    const std::vector<float> colors = AccessorReader::readFloats(document, *primitive.findAttribute("COLOR_0"));
    ASSERT_EQ(colors.size(), 240U * 4U);
    for (const float value : colors) {
        EXPECT_GE(value, 0.0F);
        EXPECT_LE(value, 1.0F);
    }
}

// Nodes keep their parents, a matrix that splits into translation, rotation and scale, a mirror by a negative scale, instances of one mesh and the default scene among several.
TEST_F(ReaderTest, ReadsTheHierarchyOfNodesAndScenes) {
    const Document document = read("hierarchy.glb");
    ASSERT_EQ(document.scenes.size(), 2U);
    EXPECT_EQ(document.scene, 1U);
    EXPECT_EQ(document.scenes[1].nodes, (std::vector<std::size_t>{0, 5}));
    const Node& root = document.nodes.at(0);
    ASSERT_TRUE(root.matrix.has_value());
    EXPECT_FALSE(root.parent.has_value());
    EXPECT_TRUE(math::Mat4::approximately(root.getLocalMatrix(), math::Mat4::compose(root.translation, root.rotation, root.scale)));
    EXPECT_EQ(root.translation.x, 1.0F);
    EXPECT_EQ(document.nodes.at(4).parent, 3U);
    EXPECT_EQ(document.nodes.at(2).scale.x, -1.0F);
    EXPECT_LT(document.nodes.at(2).getLocalMatrix().getLinear().getDeterminant(), 0.0F);
    EXPECT_EQ(document.nodes[1].mesh, document.nodes[2].mesh);
    EXPECT_EQ(document.nodes[2].mesh, document.nodes[5].mesh);
}

// A skin keeps its joints, its skeleton and its inverse bind matrices, two sets of joints and weights read as numbers, and an animation keeps every path with every interpolation and its length.
TEST_F(ReaderTest, ReadsSkinsAndAnimations) {
    const Document document = read("character.glb");
    const Skin& skin = document.skins.at(0);
    EXPECT_EQ(skin.joints, (std::vector<std::size_t>{2, 3, 5}));
    EXPECT_EQ(skin.skeleton, 4U);
    ASSERT_TRUE(skin.inverseBindMatrices.has_value());
    const std::vector<float> matrices = AccessorReader::readFloats(document, *skin.inverseBindMatrices);
    ASSERT_EQ(matrices.size(), 48U);
    EXPECT_EQ(matrices[16 + 13], -1.0F);

    const Mesh::Primitive& primitive = document.meshes.at(0).primitives.at(0);
    EXPECT_EQ(AccessorReader::readUnsigned(document, *primitive.findAttribute("JOINTS_1")).size(), 64U);
    EXPECT_EQ(document.accessors.at(*primitive.findAttribute("JOINTS_1")).componentType, Accessor::ComponentType::UnsignedShort);

    const Animation& animation = document.animations.at(0);
    EXPECT_EQ(animation.name, "Wave");
    ASSERT_EQ(animation.channels.size(), 3U);
    EXPECT_EQ(animation.channels[0].path, Animation::Path::Rotation);
    EXPECT_EQ(animation.channels[1].path, Animation::Path::Translation);
    EXPECT_EQ(animation.channels[2].path, Animation::Path::Scale);
    EXPECT_EQ(animation.samplers[0].interpolation, Animation::Interpolation::Linear);
    EXPECT_EQ(animation.samplers[1].interpolation, Animation::Interpolation::Step);
    EXPECT_EQ(animation.samplers[2].interpolation, Animation::Interpolation::CubicSpline);
    EXPECT_EQ(document.getDuration(0), 1.0F);
}

// Morph targets keep their attributes, a sparse target reads zeros with its replaced values in place, and the weights of the mesh and the node and of an animation in normalized bytes read as floats.
TEST_F(ReaderTest, ReadsMorphTargetsWithSparseData) {
    const Document document = read("morph.glb");
    const Mesh& mesh = document.meshes.at(0);
    EXPECT_EQ(mesh.getTargetCount(), 2U);
    EXPECT_EQ(mesh.weights, (std::vector<float>{0.0F, 0.5F}));
    EXPECT_EQ(document.nodes.at(0).weights, (std::vector<float>{0.25F, 0.75F}));
    EXPECT_EQ(mesh.primitives[0].targets[0][0].name, "NORMAL");
    EXPECT_EQ(mesh.primitives[0].targets[0][1].name, "POSITION");
    const std::size_t position = mesh.primitives[0].targets[0][1].accessor;
    EXPECT_EQ(AccessorReader::readFloats(document, position), (std::vector<float>{0, 0, 0, 0, 0, 0, 0, 0, 0.5F, 0, 0, 0}));
    EXPECT_EQ(AccessorReader::readFloats(document, document.animations.at(0).samplers.at(0).output), (std::vector<float>{0.0F, 1.0F, 1.0F, 0.0F}));
}

// Cameras keep both projections, a perspective without a far plane or an aspect ratio, and a light of an extension that the file only uses stays readable as its JSON.
TEST_F(ReaderTest, ReadsCamerasAndKeepsExtensionsItOnlyUses) {
    const Document document = read("camera-light.gltf");
    ASSERT_EQ(document.cameras.size(), 3U);
    EXPECT_EQ(document.cameras[0].perspective.aspectRatio, 1.5F);
    EXPECT_EQ(document.cameras[0].perspective.zfar, 100.0F);
    EXPECT_FALSE(document.cameras[1].perspective.zfar.has_value());
    EXPECT_FALSE(document.cameras[1].perspective.aspectRatio.has_value());
    EXPECT_EQ(document.cameras[2].type, Camera::Type::Orthographic);
    EXPECT_EQ(document.cameras[2].orthographic.xmag, 4.0F);
    EXPECT_EQ(document.extensionsUsed, (std::vector<std::string>{"KHR_lights_punctual"}));
    EXPECT_EQ(document.extensions.at("KHR_lights_punctual").at("lights").size(), 1U);
    EXPECT_EQ(document.nodes.at(3).extensions.at("KHR_lights_punctual").at("light"), 0);
}

// A grid of 49 materials that share one sphere keeps a material for every metalness and roughness.
TEST_F(ReaderTest, ReadsTheGridOfMaterials) {
    const Document document = read("material-grid.glb");
    ASSERT_EQ(document.materials.size(), 49U);
    EXPECT_EQ(document.materials[0].metallicFactor, 0.0F);
    EXPECT_EQ(document.materials[48].metallicFactor, 1.0F);
    EXPECT_EQ(document.materials[48].roughnessFactor, 1.0F);
    EXPECT_EQ(document.meshes[0].primitives[0].attributes, document.meshes[48].primitives[0].attributes);
}

// Every mode of primitive reads, each with a vertex count that its mode joins.
TEST_F(ReaderTest, ReadsEveryModeOfPrimitive) {
    const Document document = read("primitive-modes.gltf");
    const std::vector<Mesh::Primitive>& primitives = document.meshes.at(0).primitives;
    ASSERT_EQ(primitives.size(), 7U);
    for (std::size_t mode = 0; mode < primitives.size(); ++mode) {
        EXPECT_EQ(static_cast<std::size_t>(primitives[mode].mode), mode);
    }
}

// Accessors of every type and component type read as the numbers they store, with the padding of the columns of small matrices left out, normalized integers turned into floats, sparse values over a buffer view and an attribute of the app.
TEST_F(ReaderTest, ReadsEveryTypeOfAccessor) {
    const Document document = read("accessor-types.gltf");
    EXPECT_EQ(AccessorReader::readFloats(document, 0), (std::vector<float>{1, 2, 3, 4, 5, 6, 7, 8}));
    EXPECT_EQ(AccessorReader::readFloats(document, 1), (std::vector<float>{0, 1, 2, 3, 4, 5, 6, 7, 8, -9, -8, -7, -6, -5, -4, -3, -2, -1}));
    EXPECT_EQ(AccessorReader::readFloats(document, 2).size(), 16U);
    EXPECT_EQ(AccessorReader::readFloats(document, 3), (std::vector<float>{-1.0F, 1.0F, 0.0F, 64.0F / 127.0F}));
    EXPECT_EQ(AccessorReader::readFloats(document, 4), (std::vector<float>{0.0F, 1.0F, -1.0F}));
    EXPECT_EQ(AccessorReader::readFloats(document, 5)[0], 1.0F);
    EXPECT_EQ(AccessorReader::readUnsigned(document, 6), (std::vector<std::uint32_t>{4000000000U}));
    EXPECT_EQ(AccessorReader::readFloats(document, 8), (std::vector<float>{9, 9, 2, 2, 7, 7}));
    EXPECT_TRUE(document.meshes.at(0).primitives.at(0).findAttribute("_TEMPERATURE").has_value());
}

// Vertices that interleave in one buffer view read across its stride.
TEST_F(ReaderTest, ReadsInterleavedVertices) {
    const Document document = read("interleaved.glb");
    EXPECT_EQ(document.bufferViews.at(0).byteStride, 32U);
    EXPECT_EQ(AccessorReader::readFloats(document, 0), (std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0}));
    EXPECT_EQ(AccessorReader::readFloats(document, 1), (std::vector<float>{0, 0, 1, 0, 0, 1, 0, 0, 1}));
    EXPECT_EQ(AccessorReader::readFloats(document, 2), (std::vector<float>{0, 0, 1, 0, 0, 1}));
}

// Samplers keep every filter and wrap mode, a JPEG image in a buffer view and a PNG image in a `data:` URI decode, and materials keep every alpha mode, their cutoff, both faces and a second set of texture coordinates.
TEST_F(ReaderTest, ReadsSamplersImagesAndAlphaModes) {
    const Document document = read("samplers.gltf");
    ASSERT_EQ(document.samplers.size(), 6U);
    EXPECT_EQ(document.samplers[0].magFilter, Sampler::Filter::Nearest);
    EXPECT_EQ(document.samplers[2].minFilter, Sampler::Filter::NearestMipmapNearest);
    EXPECT_EQ(document.samplers[5].minFilter, Sampler::Filter::LinearMipmapLinear);
    EXPECT_EQ(document.samplers[0].wrapS, Sampler::Wrap::ClampToEdge);
    EXPECT_EQ(document.samplers[0].wrapT, Sampler::Wrap::MirroredRepeat);
    EXPECT_EQ(document.images[0].mimeType, "image/jpeg");
    EXPECT_EQ(document.images[1].mimeType, "image/png");
    const graphics::Image jpeg = document.decodeImage(0, 64);
    EXPECT_NEAR(jpeg.getPixel(0, 0).r * 255.0F, 192.0F, 2.0F);
    EXPECT_NEAR(jpeg.getPixel(8, 0).r * 255.0F, 64.0F, 2.0F);
    EXPECT_EQ(document.decodeImage(1, 64).getWidth(), 4);

    EXPECT_EQ(document.materials[0].alphaMode, Material::AlphaMode::Mask);
    EXPECT_EQ(document.materials[0].alphaCutoff, 0.25F);
    EXPECT_TRUE(document.materials[0].doubleSided);
    EXPECT_EQ(document.materials[0].baseColorTexture->texCoord, 1U);
    EXPECT_EQ(document.materials[1].alphaMode, Material::AlphaMode::Blend);
    EXPECT_EQ(AccessorReader::readFloats(document, *document.meshes[0].primitives[0].findAttribute("TEXCOORD_1")), (std::vector<float>{0, 0, 1, 0, 0, 1}));
}

// A texture of the extension of WebP images keeps its PNG fallback as its source and names its WebP image in its extension, which decodes like any image.
TEST_F(ReaderTest, DecodesTheWebpImageOfItsExtension) {
    const Document document = read("texture-webp.glb");
    EXPECT_EQ(document.extensionsUsed, (std::vector<std::string>{"EXT_texture_webp"}));
    const Texture& texture = document.textures.at(0);
    EXPECT_EQ(texture.source, 0U);
    EXPECT_EQ(texture.extensions.at("EXT_texture_webp").at("source"), 1);
    EXPECT_EQ(document.images.at(1).mimeType, "image/webp");
    const graphics::Image image = document.decodeImage(1, 64);
    EXPECT_EQ(image.getWidth(), 16);
    EXPECT_EQ(image.getPixel(0, 0).toRgba8(), math::Color::fromRgba8(220, 60, 40, 255).toRgba8());
}

// A decode of an image names the image and the file, and refuses an image longer than the largest side that the caller names.
TEST_F(ReaderTest, DecodesImagesWithinTheLargestSide) {
    const Document document = read("cube.glb");
    EXPECT_THROW((void)document.decodeImage(0, 8), std::runtime_error);
    EXPECT_THROW((void)document.decodeImage(3, 64), std::out_of_range);
}

} // namespace haylen::gltf
