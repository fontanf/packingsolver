def Settings( **kwargs ):
    return {
            'flags': [
                '-x', 'c++',
                '-Wall', '-Wextra', '-Werror',
                '-I', '.',
                '-I', './bazel-packingsolver/external/json/single_include',
                '-I', './bazel-packingsolver/external/googletest/googletest-release-1.8.0/googletest/include/',
                '-I', './bazel-packingsolver/external/',
                ],
            }

