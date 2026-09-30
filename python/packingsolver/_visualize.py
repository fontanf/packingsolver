"""Implementation of the 'visualize' function of each problem type submodule."""

import importlib
import io


def solution_figure(problem_type, solution, **kwargs):
    """Return the plotly figure of 'solution'.

    The solution certificate is written to an in-memory stream and read back
    by 'packingsolver.visualize.<problem_type>.figure', which receives
    'kwargs'.
    """
    try:
        module = importlib.import_module(
                "packingsolver.visualize." + problem_type)
    except ImportError as e:
        if e.name is not None and e.name.split(".")[0] in ("plotly", "numpy"):
            raise ImportError(
                    "Visualizing a solution requires plotly; install it with "
                    "'pip install packingsolver[visualization]'.") from e
        raise
    stream = io.StringIO()
    solution.write(stream)
    stream.seek(0)
    return module.figure(stream, **kwargs)
