from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.generated_tree import GeneratedTree
from testportbackend.linked_parts import LinkedParts


@dataclass(frozen=True)
class PortAppProject:
    """The CMake project of a Lua app of the test port: it finds the SDK of the engine port of the platform, builds the executable with `haylen_add_app`, which calls the app function of the port, and compiles in the parts of the plugins with their registration. The executable takes the name of the setting `executable` of the section `test-port`, or the last word of the identifier of the app."""

    SECTION = "test-port"
    PARTS = "parts"

    product: ResolvedProduct
    content: Path

    @property
    def executable(self) -> str:
        return str(self.product.primary.setting(PortAppProject.SECTION, "executable", self.product.app.identifier.rsplit(".", 1)[-1]))

    @staticmethod
    def sdk_package(product: ResolvedProduct) -> Path:
        """Returns the CMake package of the SDK that the product consumes, and refuses an SDK whose slice names a package that is not there."""
        sdk = product.primary.sdk
        package = sdk.file("cmake")
        if not (package / "haylen-config.cmake").is_file():
            raise IncompatibleSdk(
                f'The SDK `{Repository.shown(sdk.manifest.folder)}` of "{product.target_name}" is incomplete, since its slice names the CMake package `{Repository.shown(package)}`, which holds no "haylen-config.cmake". Build it again with "haylen sdk build --target {product.target_name} --config {sdk.manifest.configuration.value}".'
            )
        return package

    def write(self, tree: GeneratedTree) -> None:
        parts = LinkedParts.of(self.product.primary)
        sources = []
        for part in parts.parts:
            for source in part.sources:
                relative = f"{PortAppProject.PARTS}/{part.owner}/{source}"
                tree.copy(relative, part.folder / source)
                sources.append(relative)
        if parts.parts:
            tree.write_bytes(LinkedParts.REGISTRATION, parts.registration().encode())
            sources.append(LinkedParts.REGISTRATION)
        tree.write_bytes("CMakeLists.txt", self.cmake(sources).encode())

    def cmake(self, sources: list[str]) -> str:
        name = self.executable
        package = PortAppProject.sdk_package(self.product)
        lines = [
            "# Written by the backend of the test port from the app and the parts of its plugins.",
            "cmake_minimum_required(VERSION 3.28)",
            f"project({name} LANGUAGES CXX)",
            "",
            f'find_package(haylen CONFIG REQUIRED PATHS "{package.as_posix()}" NO_DEFAULT_PATH)',
            f'haylen_add_app({name} LUA CONTENT "{self.content.as_posix()}")',
        ]
        if sources:
            lines.append(f"target_sources({name} PRIVATE")
            lines += [f'  "{source}"' for source in sources]
            lines.append(")")
        lines.append("")
        return "\n".join(lines)
