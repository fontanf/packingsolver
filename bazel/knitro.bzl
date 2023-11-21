def _knitro_impl(repository_ctx):
    knitrodir = repository_ctx.os.environ.get("KNITRODIR", "")
    repository_ctx.symlink(knitrodir, "knitrodir")
    repository_ctx.file("BUILD", """
cc_library(
    name = "knitro",
    hdrs = [
            "knitrodir/include/knitro.h",
    ],
    strip_include_prefix = "knitrodir/include/",
    srcs = [
            "knitrodir/lib/libknitro.so",
            "knitrodir/lib/libiomp5.so",
    ],
    copts = [
            "-fopenmp",
    ],
    linkopts = [
            "-fopenmp",
            "-ldl",
            "-liomp5",
    ],
    visibility = ["//visibility:public"],
)
""")

knitro = repository_rule(
    implementation=_knitro_impl,
    local = True,
    environ = ["KNITRODIR"])
