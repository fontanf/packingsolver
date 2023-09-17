import os


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

                # Knitro
                '-DKNITRO_FOUND',
                '-I', os.getenv('KNITRODIR') + '/include/',

                # knitrocpp
                '-I', './bazel-packingsolver/external/'
                # '-I', './../'
                'knitrocpp/',

                # CGAL
                '-I', './bazel-packingsolver/external/cgal/CGAL-5.6/include/',

                ],
            }
