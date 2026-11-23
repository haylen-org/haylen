import argparse
import re
from collections.abc import Mapping
from dataclasses import dataclass

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.configuration import Configuration
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.target_id import TargetId
from haylen.platforms.backend_registry import BackendRegistry
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.terminal import Terminal


class DoctorCommand(Command):
    PATH = ("doctor",)
    HELP = 'Show what this machine has and lacks for every target, or for the one that "--target" names: its backend, its engine port, its SDKs, the tools it needs with their versions and its capabilities. It downloads nothing and accepts no license.'

    @dataclass(frozen=True)
    class Examination:
        """What the doctor found for one target: the engine port that builds the engine for it, or why none does, the installed SDKs of the current engine sources and of other sources, the installed SDKs of it that this tooling cannot use with the sentence of each, the tools it needs and its problems."""

        target: BackendRegistry.Target
        port: str
        port_version: str
        port_target: str
        port_problem: str
        current: tuple[SdkManifest, ...]
        outdated: bool
        unusable: tuple[SdkCatalog.Unusable, ...]
        unusable_sentences: tuple[str, ...]
        builds_here: bool
        tools: tuple[dict[str, object], ...]
        problems: tuple[str, ...]

        @property
        def ready(self) -> bool:
            return not self.problems and bool(self.current) and all(not tool["problem"] for tool in self.tools)

    TARGET_COLUMNS = (
        Terminal.Column("Target", style=Terminal.EXPRESSION),
        Terminal.Column("Backend"),
        Terminal.Column("Engine port", shrink=1),
        Terminal.Column("SDK", shrink=2),
        Terminal.Column("State"),
    )
    TOOL_COLUMNS = (
        Terminal.Column("Tool"),
        Terminal.Column("Version", shrink=3),
        Terminal.Column("Targets", shrink=1),
        Terminal.Column("Path", shrink=2, cut=True, stacks=True),
    )
    # The most targets that the row of a tool names one by one, past which it counts them.
    NAMED_TARGETS = 3
    # The version number in the first line that a tool prints of itself, such as "4.4.3" in "cmake version 4.4.3".
    VERSION = re.compile(r"\d+(?:\.\d+)+")

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--target", help="The target to check, every target by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        registry = session.registry
        targets = [registry.target(arguments.target)] if arguments.target else registry.targets()
        survey = session.sdks.survey()
        revision = Repository.engine_revision()
        examinations = [DoctorCommand.examine(target, registry, survey, revision) for target in targets]
        if not session.structured:
            for unusable in survey.unusable:
                session.terminal.record("\n".join([unusable.problem, *unusable.reasons]))
            problems = [*registry.problems, *(unusable.sentence(registry.targets_named_by(unusable.slices)) for unusable in survey.unusable)]
            DoctorCommand.show(session.terminal, examinations, problems, bool(arguments.target))
        targets_described = [DoctorCommand.describe(examination) for examination in examinations]
        return {"host": {"system": Host.system(), "architecture": Host.architecture()}, "problems": list(registry.problems), "targets": targets_described}

    @staticmethod
    def show(terminal: Terminal, examinations: list[DoctorCommand.Examination], machine_problems: list[str], single: bool) -> None:
        """Prints the host, a table of the targets with their state, a table of the tools they need, each once with the targets that need it, and every problem once: those of the backends, the engine ports and the SDKs of this machine that this tooling cannot use, then those of the targets."""
        terminal.group(f"Haylen {Repository.engine_version()} on {Host.system()} {Host.architecture()}")
        terminal.group("Targets", len(examinations))
        terminal.table(DoctorCommand.TARGET_COLUMNS, [DoctorCommand.target_row(examination) for examination in examinations])
        if single:
            target = examinations[0].target
            capabilities = ", ".join(sorted(capability.value for capability in target.platform.capabilities))
            terminal.rows([("Description", target.description.description), ("Capabilities", capabilities)])

        tools = DoctorCommand.tool_rows(examinations)
        if tools:
            # The tools of one target need no column that names their targets.
            columns = [column for column in DoctorCommand.TOOL_COLUMNS if not single or column.title != "Targets"]
            rows = [(tool, version, path) for tool, version, _, path in tools] if single else tools
            terminal.group("Tools", len(tools))
            terminal.table(columns, rows)

        problems = list(dict.fromkeys([*machine_problems, *(problem for examination in examinations for problem in DoctorCommand.problems_of(examination))]))
        if problems:
            terminal.group("Problems", len(problems))
            for problem in problems:
                terminal.item(Terminal.Sign.FAILED, problem)

    @staticmethod
    def target_row(examination: DoctorCommand.Examination) -> tuple[str | Terminal.Cell, ...]:
        description = examination.target.description
        port = examination.port
        if port and examination.port_target != description.name:
            port = f"{port} as {examination.port_target}"
        sdk = ", ".join(manifest.configuration.value for manifest in examination.current)
        if not sdk and examination.outdated:
            sdk = "Outdated"
        return (description.name, examination.target.platform.name, port, sdk, DoctorCommand.state(examination))

    @staticmethod
    def state(examination: DoctorCommand.Examination) -> Terminal.Cell:
        """The state of a target: whether this machine builds it, what keeps it from building and whether its SDK still has to be built, which the first app build does."""
        if not examination.builds_here:
            return Terminal.Cell("Other host", Terminal.Sign.UNREACHABLE)
        if examination.unusable:
            return Terminal.Cell(examination.unusable[0].state, Terminal.Sign.UNREACHABLE)
        if examination.port_problem or examination.problems or any(tool["problem"] for tool in examination.tools):
            return Terminal.Cell("Problems", Terminal.Sign.FAILED)
        if not examination.current:
            return Terminal.Cell("No SDK yet", Terminal.Sign.IDLE)
        return Terminal.Cell("Ready", Terminal.Sign.READY)

    @staticmethod
    def tool_rows(examinations: list[DoctorCommand.Examination]) -> list[tuple[str | Terminal.Cell, ...]]:
        """A row for every tool, once for every version and path that the targets found it at, with the targets that need it."""
        found: dict[tuple, list[str]] = {}
        tools: dict[tuple, dict[str, object]] = {}
        for examination in examinations:
            for tool in examination.tools:
                key = (tool["name"], tool["version"], tool["path"], tool["problem"])
                tools[key] = tool
                found.setdefault(key, []).append(examination.target.description.name)
        rows = []
        for key, tool in tools.items():
            names = found[key]
            targets = ", ".join(names) if len(names) <= DoctorCommand.NAMED_TARGETS else f"{len(names)} targets"
            rows.append((DoctorCommand.tool_cell(tool), DoctorCommand.version(tool), targets, DoctorCommand.tool_path(tool)))
        return rows

    @staticmethod
    def tool_cell(tool: Mapping[str, object]) -> Terminal.Cell:
        if tool["problem"]:
            return Terminal.Cell(str(tool["name"]), Terminal.Sign.FAILED)
        if tool["path"] is None:
            return Terminal.Cell(str(tool["name"]), Terminal.Sign.IDLE)
        return Terminal.Cell(str(tool["name"]), Terminal.Sign.READY)

    @staticmethod
    def version(tool: Mapping[str, object]) -> str:
        """The version number that a tool reports, or nothing when it reports none."""
        if not tool["version"]:
            return ""
        match = DoctorCommand.VERSION.search(str(tool["version"]))
        return match[0] if match else str(tool["version"])

    @staticmethod
    def tool_path(tool: Mapping[str, object]) -> str:
        """Where a tool is, or, for a pinned tool that is not in `.tools` yet, that the first build that needs it downloads it."""
        if tool["path"] is None and not tool["problem"]:
            return "Downloads with the first build"
        return str(tool["path"] or "")

    @staticmethod
    def problems_of(examination: DoctorCommand.Examination) -> list[str]:
        tools = [str(tool["problem"]) for tool in examination.tools if tool["problem"]]
        return [*([examination.port_problem] if examination.port_problem else []), *examination.problems, *tools]

    @staticmethod
    def examine(target: BackendRegistry.Target, registry: BackendRegistry, survey: SdkCatalog.Survey, revision: str) -> DoctorCommand.Examination:
        description = target.description
        port = port_version = port_target = port_problem = ""
        try:
            found, matched = registry.port_for(description)
            port = found.description.name
            port_version = found.description.version
            port_target = matched.name
        except HaylenError as error:
            port_problem = error.message

        slices = [manifest for manifest in survey.manifests for entry in manifest.slices if entry.system == description.system and entry.environment is description.environment]
        current = tuple(manifest for manifest in slices if manifest.revision == revision)
        named = (description.system, description.environment.value)
        unusable = tuple(sdk for sdk in survey.unusable if named in sdk.slices)
        sentences = tuple(sdk.sentence(registry.targets_named_by(sdk.slices)) for sdk in unusable)
        builds_here = Host.system() in description.hosts
        problems = list(sentences)
        tools: tuple[dict[str, object], ...] = ()
        if not builds_here:
            problems.append(f'The target "{description.name}" builds only on {", ".join(sorted(description.hosts))}, and this machine runs "{Host.system()}".')
        else:
            architecture = description.default_architectures[0]
            report = target.backend.check_environment(TargetId(description.system, architecture, description.abis[architecture], description.environment, Configuration.DEBUG, description.toolchain))
            tools = tuple(tool.describe() for tool in report.tools)
            problems += report.problems
        return DoctorCommand.Examination(target, port, port_version, port_target, port_problem, current, bool(slices) and not current, unusable, sentences, builds_here, tools, tuple(problems))

    @staticmethod
    def describe(examination: DoctorCommand.Examination) -> dict[str, object]:
        """The result of one target in structured output."""
        target = examination.target
        description = target.description
        port = examination.port_problem or f'"{examination.port}" {examination.port_version} builds the engine for it as "{examination.port_target}".'
        if examination.unusable and not examination.current:
            sdk = examination.unusable_sentences[0]
        elif examination.current:
            sdk = ", ".join(f"`{Repository.shown(manifest.folder)}` ({manifest.configuration.value})" for manifest in examination.current)
        elif examination.outdated:
            sdk = f'The installed SDKs were built from other engine sources. Build them again with "haylen sdk build --target {description.name}".'
        else:
            sdk = f'No SDK is installed. Build it with "haylen sdk build --target {description.name}".'
        return {
            "target": description.describe(),
            "backend": target.platform.name,
            "capabilities": sorted(capability.value for capability in target.platform.capabilities),
            "port": port,
            "sdk": sdk,
            "tools": list(examination.tools),
            "problems": list(examination.problems),
            "ready": examination.ready,
        }
