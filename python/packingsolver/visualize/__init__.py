"""Plotly figures of PackingSolver solutions, one module per problem type.

Each module provides 'figure(certificate, ...)', which reads a solution
certificate (the output of 'Solution.write') from a path or a text stream.
These modules only depend on plotly (and numpy for 'irregular'), not on the
compiled extension, so that the scripts in 'scripts/' can load them from a
source checkout.

From the solver's submodules, 'visualize(solution, ...)' builds the figure of
a solution directly, without writing any file:

    import packingsolver.rectangle as psr

    output = psr.optimize(instance, parameters)
    psr.visualize(output.solution).show()
"""
