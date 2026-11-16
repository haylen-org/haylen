from haylen.core.repository import Repository
from haylen.engine.source_format import SourceFormat


class FormatTest:
    """The format check finds multi-line lambdas outside "clang-format off" markers, which the array forms of "operator new" and "operator delete" never are, and leaves the files of the plugin template whose names hold tokens to "haylen plugin new"."""

    def test_multi_line_lambdas_need_markers_while_the_array_forms_of_new_and_delete_do_not(self, tmp_path):
        source = tmp_path / "Source.cpp"
        lines = [
            "void* operator new[](std::size_t size) {",
            "}",
            "void operator delete[](void* memory, std::size_t) noexcept {",
            "}",
            "const auto call = [this](int value) {",
            "};",
            "// clang-format off",
            "const auto guarded = [&value] {",
            "};",
            "// clang-format on",
        ]
        source.write_text("\n".join(lines), encoding="utf-8")
        findings = SourceFormat.unguarded_lambdas([source])
        assert len(findings) == 1
        assert "Source.cpp:5" in findings[0]

    def test_the_files_of_the_plugin_template_with_tokens_stay_out(self):
        sources = SourceFormat.sources()
        assert sources
        assert not [path for path in sources if "{{" in path.name]
        assert all(path.is_relative_to(Repository.ROOT) for path in sources)
