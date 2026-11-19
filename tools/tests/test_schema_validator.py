from importlib import resources

from haylen.core.schema_validator import SchemaValidator


class SchemaValidatorTest:
    def test_a_schema_of_the_tooling_is_read_once_so_a_file_that_is_rewritten_meanwhile_never_reaches_a_validation(self, tmp_path, monkeypatch):
        validator = SchemaValidator.of("plugin.schema.json")
        # An editor that writes the schema in place leaves it empty for a moment.
        (tmp_path / "plugin.schema.json").write_text("", encoding="utf-8")
        monkeypatch.setattr(resources, "files", lambda package: tmp_path)

        assert SchemaValidator.of("plugin.schema.json") is validator
