def Settings(**kwargs):
    return {
            'flags': [
                '-x', 'c++',
                '-Wall', '-Wextra', '-Werror',
                '-I', '.',

                '-I', './bazel-packingsolver/external/'
                'json/single_include/',

                '-I', './bazel-packingsolver/external/'
                'googletest/googletest/include/',

                '-I', './bazel-packingsolver/external/'
                'boost/',

                # optimizationtools
                '-I', './bazel-packingsolver/external/'
                # '-I', './../'
                'optimizationtools/',

                # treesearchsolver
                '-I', './bazel-packingsolver/external/'
                # '-I', './../'
                'treesearchsolver/',

                # knapsacksolver
                '-I', './bazel-packingsolver/external/'
                # '-I', './../'
                'knapsacksolver/',

                # columngenerationsolver
                '-I', './bazel-packingsolver/external/'
                # '-I', './../'
                'columngenerationsolver/',

                # AMPL
                '-DAMPL_FOUND',
                '-I', '/home/florian/Programmes/ampl.linux-intel64/amplapi/include/'

                ],
            }
