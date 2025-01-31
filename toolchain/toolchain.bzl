"""Rules for toolchain definiitions."""

load("//rules:sci.bzl", "SciSystemInfo")

visibility("public")

def _compiled_toolchain_config_impl(ctx):
    toolchain_info = platform_common.ToolchainInfo(
        scic = ctx.executable.scic,
        systems = [system[SciSystemInfo] for system in ctx.attr.systems],
    )
    return [
        toolchain_info,
    ]

compiled_toolchain_config = rule(
    implementation = _compiled_toolchain_config_impl,
    attrs = {
        "scic": attr.label(allow_single_file = True, executable = True, cfg = "exec", mandatory = True),
        "systems": attr.label_list(mandatory = True, providers = [SciSystemInfo]),
    },
)
