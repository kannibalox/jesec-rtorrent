load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(
      ["**"]
    ),
)

configure_make(
    name = "libpq",
    configure_in_place = True,
    args = [
        "-j4",  # Make it faster by using more processes.
    ],
    out_include_dir = "src/include/",
    visibility = ["//visibility:public"],
    lib_source = ":all",
    out_static_libs = [
        "libecpg.a",
        "libecpg_compat.a",
        "libpgcommon.a",
        "libpgcommon_shlib.a",
        "libpgfeutils.a",
        "libpgport.a",
        "libpgport_shlib.a",
        "libpgtypes.a",
        "libpq.a",
    ],
)
