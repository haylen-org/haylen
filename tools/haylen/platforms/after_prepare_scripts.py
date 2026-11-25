import sys
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.product_folders import ProductFolders


class AfterPrepareScripts:
    """The scripts that `afterPrepare` of the sections of `platforms` in `haylen.json` name, which change the generated project of an app in what no key of `haylen.json` reaches. Each one is a Python script of the app folder that the tooling runs with its own Python after every preparation that writes the project, in the order of the sections, from the app folder, with the folder of the project as its argument and the variables `HAYLEN_PROJECT`, `HAYLEN_TARGET`, `HAYLEN_PLATFORM` and `HAYLEN_APP` in its environment. It runs the same way in every configuration, so a Debug and a Release build declare the same, and its content joins the inputs of the preparation, so a project stays the same for the same inputs."""

    KEY = "afterPrepare"

    @staticmethod
    def of(product: ResolvedProduct) -> tuple[tuple[str, Path], ...]:
        """The scripts of the sections that apply to the target, each with its section."""
        app = product.app
        return tuple((section, app.folder / str(values[AfterPrepareScripts.KEY])) for section, values in product.primary.settings.items() if AfterPrepareScripts.KEY in values)

    @staticmethod
    def run(context: ToolingContext, product: ResolvedProduct, folders: ProductFolders) -> None:
        """Runs every script on the project, and fails with a sentence that names the script that failed."""
        for section, script in AfterPrepareScripts.of(product):
            context.terminal.step(f'Running `{Repository.shown(script)}` of "platforms.{section}.{AfterPrepareScripts.KEY}" on the project')
            environment = {
                **context.environment,
                "HAYLEN_PROJECT": str(folders.project),
                "HAYLEN_TARGET": product.target_name,
                "HAYLEN_PLATFORM": product.primary.platform,
                "HAYLEN_APP": str(product.app.folder),
            }
            try:
                context.process.run([sys.executable, script, folders.project], cwd=product.app.folder, env=environment)
            except BuildFailed as error:
                raise BuildFailed(
                    f'The script `{Repository.shown(script)}` of "platforms.{section}.{AfterPrepareScripts.KEY}" in `{product.app.origin}` failed on the project of "{product.target_name}", so the preparation stops. Its output above shows why.'
                ) from error
