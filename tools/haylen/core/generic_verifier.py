from haylen.core.repository import Repository
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport


class GenericVerifier:
    """The checks of every built product, whatever its platform: the product exists and holds something, it has a component for every target of its product and only for those, and every component and auxiliary file it declares exists. The backend adds its own checks to the same report."""

    SOURCE = "core"

    @staticmethod
    def verify(product: BuildProduct) -> VerificationReport:
        checks: list[VerificationCheck] = []
        shown = f"`{Repository.shown(product.path)}`"
        if not product.path.exists():
            checks.append(VerificationCheck.failed("product", f"The product {shown} does not exist, although the build succeeded.", GenericVerifier.SOURCE))
            return VerificationReport(tuple(checks))
        empty = (product.path.is_dir() and not any(product.path.iterdir())) or (product.path.is_file() and product.path.stat().st_size == 0)
        if empty:
            checks.append(VerificationCheck.failed("product", f"The product {shown} is empty.", GenericVerifier.SOURCE))
        else:
            checks.append(VerificationCheck.passed("product", f"The product {shown} exists.", GenericVerifier.SOURCE))

        targets = {project.target for project in product.prepared.product.projects}
        covered = {component.target for component in product.components}
        missing = sorted(target.text for target in targets - covered)
        foreign = sorted(target.text for target in covered - targets)
        if missing:
            checks.append(VerificationCheck.failed("targets", f"The product has no component for {', '.join(missing)}.", GenericVerifier.SOURCE))
        if foreign:
            checks.append(VerificationCheck.failed("targets", f"The product has components for {', '.join(foreign)}, which it was not built for.", GenericVerifier.SOURCE))
        if not missing and not foreign:
            checks.append(VerificationCheck.passed("targets", f"The product serves {', '.join(sorted(target.text for target in targets))}.", GenericVerifier.SOURCE))

        for path in (*(component.path for component in product.components), *product.auxiliary):
            if not path.exists():
                checks.append(VerificationCheck.failed("files", f"The product declares `{Repository.shown(path)}`, which does not exist.", GenericVerifier.SOURCE))
        return VerificationReport(tuple(checks))
