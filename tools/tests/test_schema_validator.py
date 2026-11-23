from importlib import resources

from haylen.core.schema_validator import SchemaValidator


class SchemaValidatorTest:
    def test_a_schema_of_the_tooling_is_read_once_so_a_file_that_is_rewritten_meanwhile_never_reaches_a_validation(self, tmp_path, monkeypatch):
        validator = SchemaValidator.of("plugin.schema.json")
        # An editor that writes the schema in place leaves it empty for a moment.
        (tmp_path / "plugin.schema.json").write_text("", encoding="utf-8")
        monkeypatch.setattr(resources, "files", lambda package: tmp_path)

        assert SchemaValidator.of("plugin.schema.json") is validator

    def test_a_problem_names_the_items_of_a_list_in_words_and_counts_them_from_one(self):
        assert SchemaValidator.subject(["content", "delivery", 2, "mode"]) == 'The key "mode" of item 3 of "content.delivery"'
        assert SchemaValidator.subject(["plugins", 0]) == 'Item 1 of "plugins"'
        assert SchemaValidator.subject(["window", "title"]) == 'The key "window.title"'
        assert SchemaValidator.subject([]) == "The file"

        schema = {"type": "object", "properties": {"delivery": {"type": "array", "items": {"type": "object", "required": ["mode"]}}}}
        assert SchemaValidator(schema).problems({"delivery": [{"mode": "required"}, {}]}) == ['Item 2 of "delivery" needs "mode".']
