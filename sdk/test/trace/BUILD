load("//bazel:otel_cc_benchmark.bzl", "otel_cc_benchmark")

cc_test(
    name = "tracer_provider_test",
    srcs = [
        "tracer_provider_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "span_data_test",
    srcs = [
        "span_data_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "simple_processor_test",
    srcs = [
        "simple_processor_test.cc",
    ],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "batch_span_processor_test",
    srcs = [
        "batch_span_processor_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_test",
    srcs = [
        "tracer_test.cc",
    ],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "always_on_sampler_test",
    srcs = [
        "always_on_sampler_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "always_off_sampler_test",
    srcs = [
        "always_off_sampler_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parent_or_else_sampler_test",
    srcs = [
        "parent_or_else_sampler_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "probability_sampler_test",
    srcs = [
        "probability_sampler_test.cc",
    ],
    deps = [
        "//sdk/src/common:random",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "attribute_utils_test",
    srcs = [
        "attribute_utils_test.cc",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

otel_cc_benchmark(
    name = "sampler_benchmark",
    srcs = ["sampler_benchmark.cc"],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/trace",
    ],
)
