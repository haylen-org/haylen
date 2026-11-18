from haylen.toolchains.llvm import Llvm


class LlvmTest:
    def test_looks_in_every_version_that_the_packages_installed_the_newest_first(self, tmp_path, monkeypatch):
        for name in ("llvm-9", "llvm-23", "llvm-18", "llvm-tools", "python3"):
            (tmp_path / name).mkdir()
        monkeypatch.setattr(Llvm, "VERSIONED_ROOT", tmp_path)

        folders = Llvm.folders(None, {})

        assert folders[len(Llvm.PACKAGE_FOLDERS) :] == [tmp_path / "llvm-23" / "bin", tmp_path / "llvm-18" / "bin", tmp_path / "llvm-9" / "bin"]
