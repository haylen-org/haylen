import argparse
import json
import urllib.error
import urllib.request
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.repository import Repository
from haylen.development.development_record import DevelopmentRecord
from haylen.development.development_server import DevelopmentServer


class AppCaptureCommand(Command):
    PATH = ("app", "capture")
    HELP = 'Capture the next frames of the GPU in an app that "haylen app run" or "haylen app serve" plays in development in another terminal, with the capture tool of its graphics API, and print the path of each capture file, which the apps write into their storage.'

    # The capture waits for the apps to answer and to end their captures, and a little longer for the server to answer.
    ANSWER_SECONDS = DevelopmentServer.CONSOLE_ANSWER_SECONDS + DevelopmentServer.GPU_CAPTURE_SECONDS + 5

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("--frames", type=AppCaptureCommand.frames, default=1, help="Frames that the capture records, 1 by default.")

    @staticmethod
    def frames(text: str) -> int:
        """Reads the number of frames of a capture, which is a whole number of at least 1."""
        if not text.isdigit() or int(text) < 1:
            raise argparse.ArgumentTypeError(f'"{text}" is no number of frames. Name at least 1 frame.')
        return int(text)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        shown = Repository.shown(folder)
        record = DevelopmentRecord.read(folder)
        if record is None:
            raise DevelopmentFailed(f'No run of the app `{shown}` serves its development server. Run it with "haylen app run {shown}" and capture while it runs.')

        frames = arguments.frames
        session.terminal.step(f"Capturing the next {frames} {'frame' if frames == 1 else 'frames'} of the GPU in the running app")
        results = AppCaptureCommand.request(record, frames, shown)
        if not results:
            raise DevelopmentFailed("No app is connected to the development server of the run. Wait until the app connects, which its log says, and capture again.")

        for result in results:
            if result.get("ok"):
                session.terminal.success(DevelopmentServer.capture_written(result))
            else:
                session.terminal.error(result.get("error", "The GPU capture failed."))
        if not any(result.get("ok") for result in results):
            raise DevelopmentFailed("No app wrote a GPU capture, and the errors above say why.")
        return {"results": results}

    @staticmethod
    def request(record: DevelopmentRecord, frames: int, shown: str) -> list[dict]:
        """Asks the development server of the run for the captures of its apps and returns what each app answered."""
        address = f"{record.address}{DevelopmentServer.GPU_CAPTURE_PATH}?token={record.token}"
        body = json.dumps({"frames": frames}).encode()
        request = urllib.request.Request(address, data=body, headers={"Content-Type": "application/json"}, method="POST")
        # The server is on this machine, so no proxy of the environment stands between them.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        try:
            with opener.open(request, timeout=AppCaptureCommand.ANSWER_SECONDS) as response:
                return json.loads(response.read())["results"]
        except urllib.error.HTTPError as error:
            answer = json.loads(error.read() or b"{}")
            raise DevelopmentFailed(answer.get("error", f"The development server refused the capture with the status {error.code}.")) from error
        except OSError as error:
            raise DevelopmentFailed(
                f'The development server of the app at `{record.address}` does not answer, so its run has ended. Run the app with "haylen app run {shown}" and capture while it runs.'
            ) from error
