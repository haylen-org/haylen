from tests.support.tool_script import ToolScript


class GltfFixturesTest:
    """The glTF test files of the engine are the ones that `tools/gltf-fixtures.py` writes, so every file can be made again and none is left over."""

    def test_the_test_files_are_the_ones_the_tool_writes(self):
        assert ToolScript.load("gltf-fixtures.py").GltfFixtures.differences() == []

    def test_every_written_file_is_named_with_dashes(self):
        names = ToolScript.load("gltf-fixtures.py").GltfFixtures.write()
        assert [name for name in names if "_" in name or " " in name] == []
