load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

filegroup(
    name = "all",
    srcs = glob(
      ["**"]
    ),
)

make(
   name="lua",
   lib_source = ":all",
   visibility = ["//visibility:public"],
   targets = ["linux", "install"],
   out_static_libs = ["liblua.a"],
   args = [
     "INSTALL_TOP=$$INSTALLDIR$$",
     "CC=clang",
   ]
)
