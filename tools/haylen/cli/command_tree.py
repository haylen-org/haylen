from haylen.cli.commands.app_build import AppBuildCommand
from haylen.cli.commands.app_capture import AppCaptureCommand
from haylen.cli.commands.app_package import AppPackageCommand
from haylen.cli.commands.app_prepare import AppPrepareCommand
from haylen.cli.commands.app_run import AppRunCommand
from haylen.cli.commands.app_serve import AppServeCommand
from haylen.cli.commands.app_shaders import AppShadersCommand
from haylen.cli.commands.app_sign import AppSignCommand
from haylen.cli.commands.app_verify import AppVerifyCommand
from haylen.cli.commands.app_zip import AppZipCommand
from haylen.cli.commands.content_build import ContentBuildCommand
from haylen.cli.commands.content_compact import ContentCompactCommand
from haylen.cli.commands.content_diff import ContentDiffCommand
from haylen.cli.commands.content_inspect import ContentInspectCommand
from haylen.cli.commands.content_keys import ContentKeysCommand
from haylen.cli.commands.content_publish import ContentPublishCommand
from haylen.cli.commands.content_verify import ContentVerifyCommand
from haylen.cli.commands.devices import DevicesCommand
from haylen.cli.commands.doctor import DoctorCommand
from haylen.cli.commands.engine_bench import EngineBenchCommand
from haylen.cli.commands.engine_build import EngineBuildCommand
from haylen.cli.commands.engine_clean import EngineCleanCommand
from haylen.cli.commands.engine_configure import EngineConfigureCommand
from haylen.cli.commands.engine_consumers import EngineConsumersCommand
from haylen.cli.commands.engine_coverage import EngineCoverageCommand
from haylen.cli.commands.engine_format import EngineFormatCommand
from haylen.cli.commands.engine_fuzz import EngineFuzzCommand
from haylen.cli.commands.engine_tools import EngineToolsCommand
from haylen.cli.commands.new import NewCommand
from haylen.cli.commands.plugin_add import PluginAddCommand
from haylen.cli.commands.plugin_list import PluginListCommand
from haylen.cli.commands.plugin_new import PluginNewCommand
from haylen.cli.commands.plugin_remove import PluginRemoveCommand
from haylen.cli.commands.plugin_update import PluginUpdateCommand
from haylen.cli.commands.project_inspect import ProjectInspectCommand
from haylen.cli.commands.sdk_build import SdkBuildCommand
from haylen.cli.commands.sdk_join import SdkJoinCommand
from haylen.cli.commands.sdk_list import SdkListCommand
from haylen.cli.commands.serve import ServeCommand
from haylen.cli.commands.test import TestCommand


class CommandTree:
    """The commands of the command line, by the words that name them, and the help of the groups that hold them. A command that only some targets serve, such as packaging, asks the backend of its target through the pipeline, so a new platform adds no command."""

    GROUPS = {
        ("app",): "Prepare, build, verify, run and package an app for a target.",
        ("engine",): "Configure, build, benchmark, fuzz, measure, format and clean the engine itself, and check that other projects consume it.",
        ("content",): "Build, verify, inspect, compare and publish the protected releases of an app, and show its keys.",
        ("plugin",): "Add, update, remove, list and create the plugins of an app.",
        ("project",): "Inspect the resolved project of an app.",
        ("sdk",): "Build, join and list the engine SDKs that apps build against.",
    }

    COMMANDS = (
        NewCommand,
        DoctorCommand,
        DevicesCommand,
        PluginAddCommand,
        PluginUpdateCommand,
        PluginRemoveCommand,
        PluginListCommand,
        PluginNewCommand,
        ProjectInspectCommand,
        SdkBuildCommand,
        SdkJoinCommand,
        SdkListCommand,
        AppPrepareCommand,
        AppBuildCommand,
        AppVerifyCommand,
        AppRunCommand,
        AppPackageCommand,
        AppSignCommand,
        AppZipCommand,
        AppShadersCommand,
        AppServeCommand,
        AppCaptureCommand,
        ServeCommand,
        ContentBuildCommand,
        ContentVerifyCommand,
        ContentInspectCommand,
        ContentDiffCommand,
        ContentPublishCommand,
        ContentCompactCommand,
        ContentKeysCommand,
        TestCommand,
        EngineConfigureCommand,
        EngineBuildCommand,
        EngineBenchCommand,
        EngineFuzzCommand,
        EngineCoverageCommand,
        EngineFormatCommand,
        EngineCleanCommand,
        EngineToolsCommand,
        EngineConsumersCommand,
    )
