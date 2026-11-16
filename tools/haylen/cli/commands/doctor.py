import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.configuration import Configuration
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.target_id import TargetId
from haylen.platforms.backend_registry import BackendRegistry


class DoctorCommand(Command):
    PATH = ("doctor",)
    HELP = 'Show what this machine has and lacks for every target, or for the one that "--target" names: its backend, its engine port, its SDKs, the tools it needs with their versions and its capabilities. It downloads nothing and accepts no license.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--target", help="The target to check, every target by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        registry = session.registry
        terminal = session.terminal
        for problem in registry.problems:
            terminal.warning(problem)
        targets = [registry.target(arguments.target)] if arguments.target else registry.targets()
        manifests = session.sdks.manifests()
        revision = Repository.engine_revision()

        results = []
        for target in targets:
            result = DoctorCommand.examine(target, registry, manifests, revision)
            results.append(result)
            if session.structured:
                continue
            terminal.heading(f"{target.description.name}: {target.description.description}")
            rows = [
                ("Backend", f'"{target.platform.name}" {target.platform.version}'),
                ("Capabilities", ", ".join(result["capabilities"])),
                ("Engine port", result["port"]),
                ("SDK", result["sdk"]),
            ]
            rows += [(tool["name"], tool["problem"] or f"{tool['version'] or 'Found'} at `{tool['path']}`") for tool in result["tools"]]
            terminal.rows(rows)
            for problem in result["problems"]:
                terminal.warning(problem)
        return {"host": {"system": Host.system(), "architecture": Host.architecture()}, "problems": list(registry.problems), "targets": results}

    @staticmethod
    def examine(target: BackendRegistry.Target, registry: BackendRegistry, manifests: list, revision: str) -> dict[str, object]:
        description = target.description
        result: dict[str, object] = {
            "target": description.describe(),
            "backend": target.platform.name,
            "capabilities": sorted(capability.value for capability in target.platform.capabilities),
        }
        problems: list[str] = []
        try:
            port, port_target = registry.port_for(description)
            result["port"] = f'"{port.description.name}" {port.description.version} builds the engine for it as "{port_target.name}".'
        except HaylenError as error:
            result["port"] = error.message

        slices = [manifest for manifest in manifests for entry in manifest.slices if entry.system == description.system and entry.environment is description.environment]
        current = [manifest for manifest in slices if manifest.revision == revision]
        if current:
            result["sdk"] = ", ".join(f"`{Repository.shown(manifest.folder)}` ({manifest.configuration.value})" for manifest in current)
        elif slices:
            result["sdk"] = f'The installed SDKs were built from other engine sources. Build them again with "haylen sdk build --target {description.name}".'
        else:
            result["sdk"] = f'No SDK is installed. Build it with "haylen sdk build --target {description.name}".'

        if Host.system() not in description.hosts:
            problems.append(f'The target "{description.name}" builds only on {", ".join(sorted(description.hosts))}, and this machine runs "{Host.system()}".')
            result["tools"] = []
        else:
            architecture = description.default_architectures[0]
            report = target.backend.check_environment(TargetId(description.system, architecture, description.abis[architecture], description.environment, Configuration.DEBUG, description.toolchain))
            result["tools"] = [tool.describe() for tool in report.tools]
            problems += report.problems
        result["problems"] = problems
        result["ready"] = not problems and bool(current) and all(not tool["problem"] for tool in result["tools"])
        return result
