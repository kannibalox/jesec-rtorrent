load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(
      ["**"]
    ),
)

configure_make(
    name = "libpqxx",
    args = [
        "-j4",  # Make it faster by using more processes.
    ],
    visibility = ["//visibility:public"],
    configure_options = [
        "--disable-documentation",
    ],
    copts = ["-std=c++17"],
    deps = ["@@libpq//:libpq"],
    lib_source = ":all",
    out_static_libs = [
        "libpqxx.a",
    ],
)
