"""Rules to build with the SC compiler."""

visibility("public")

SciSystemInfo = provider(
    "Configuration info for a particular SCI system.",
    fields = {
        "target_vm": "The target VM to use, as a string.",
        "defines": "A set of defines to use.",
        "system_header": "The system header file to use.",
        "headers": "A depset of headers that are available to include.",
    },
)

_SciHeaderSetInfo = provider(
    "Configuration info for a set of SCI headers.",
    fields = {
        "headers": "A depset of headers that are available to include.",
    },
)

_SciBuildEnvInfo = provider(
    "The base set of files needed to build a SCI source file.",
    fields = {
        "selector_file": "The selector file to use.",
        "classdef_file": "The class definition file to use.",
        "target_vm": "A target_vm string to use.",
        "game_header": "A depset of game headers to include.",
    },
)

_SciScriptInfo = provider(
    "A compiled SCI script.",
    fields = {
        "src": "The source file to compile.",
        "script_num": "The script number to use.",
        "headers": "A depset of header files.",
    },
)

def _sci_system_impl(ctx):
    headers = depset(
        [ctx.file.system_header],
        transitive = [
            dep[_SciHeaderSetInfo].headers
            for dep in ctx.attr.deps
        ],
    )
    return [
        SciSystemInfo(
            target_vm = ctx.attr.target_vm,
            defines = ctx.attr.defines,
            system_header = ctx.file.system_header,
            headers = headers,
        ),
    ]

sci_system = rule(
    implementation = _sci_system_impl,
    attrs = {
        "target_vm": attr.string(values = ["1.1", "2"], default = "1.1"),
        "defines": attr.string_list(),
        "system_header": attr.label(allow_single_file = True, mandatory = True),
        "deps": attr.label_list(providers = [_SciHeaderSetInfo]),
    },
)

def _sci_build_env_impl(ctx):
    return [
        _SciBuildEnvInfo(
            selector_file = ctx.file.selector,
            classdef_file = ctx.file.classdef,
            target_vm = ctx.attr.target_vm,
            game_header = ctx.file.game_header,
        ),
    ]

sci_build_env = rule(
    implementation = _sci_build_env_impl,
    attrs = {
        "selector": attr.label(allow_single_file = True, mandatory = True),
        "classdef": attr.label(allow_single_file = True),
        "game_header": attr.label(allow_single_file = [".sh"]),
        "target_vm": attr.string(values = ["1.1", "2"], default = "1.1"),
    },
)

def _sci_headers_impl(ctx):
    new_headers = depset(
        ctx.files.hdrs,
        transitive = [
            dep[_SciHeaderSetInfo].headers
            for dep in ctx.attr.deps
        ],
    )
    return [
        _SciHeaderSetInfo(
            headers = new_headers,
        ),
    ]

sci_headers = rule(
    implementation = _sci_headers_impl,
    attrs = {
        "hdrs": attr.label_list(allow_files = True, mandatory = True),
        "deps": attr.label_list(providers = [_SciHeaderSetInfo]),
    },
)

def _sci_script_impl(ctx):
    return [
        _SciScriptInfo(
            src = ctx.file.src,
            script_num = ctx.attr.script_num,
            headers = depset(transitive = [
                dep[_SciHeaderSetInfo].headers
                for dep in ctx.attr.deps
            ]),
        ),
    ]

sci_script = rule(
    implementation = _sci_script_impl,
    attrs = {
        "src": attr.label(allow_single_file = True, mandatory = True),
        "script_num": attr.int(mandatory = True),
        "deps": attr.label_list(providers = [_SciHeaderSetInfo]),
    },
)

def _sci_binary_impl(ctx):
    sci_toolchain = ctx.toolchains["//toolchain:sci_compiler_toolchain_type"]
    build_env_info = ctx.attr.build_env[_SciBuildEnvInfo]
    possible_systems = [
        system
        for system in sci_toolchain.systems
        if system.target_vm == build_env_info.target_vm
    ]
    if len(possible_systems) != 1:
        fail("Could not find a system for target VM: {0}".format(build_env_info.target_vm))
    system_info = possible_systems[0]
    out_dir = ctx.actions.declare_directory(ctx.label.name)
    outputs = [out_dir]
    inputs = [
        build_env_info.selector_file,
        build_env_info.classdef_file,
        system_info.system_header,
        build_env_info.game_header,
    ]
    srcs = []
    hdrs = depset(transitive = [
        script[_SciScriptInfo].headers
        for script in ctx.attr.srcs
    ] + [system_info.headers]).to_list()
    include_dirs = [hdr.dirname for hdr in hdrs]
    for src_value in ctx.attr.srcs:
        src_info = src_value[_SciScriptInfo]
        srcs.append(src_info.src)

    if system_info.target_vm == "2":
        target_env = "SCI_2"
    elif system_info.target_vm == "1.1":
        target_env = "SCI_1_1"
    else:
        fail("Unknown target VM: {0}".format(build_env_info.target_vm))

    ctx.actions.run(
        inputs = inputs + srcs + hdrs,
        outputs = outputs,
        executable = sci_toolchain.scic,
        arguments = [
            ctx.actions.args()
                .add("-a")
                .add("-u")
                .add("-l")
                .add_all(system_info.defines, before_each = "-D")
                .add("-t", target_env)
                .add("--selector_file", build_env_info.selector_file)
                .add("--classdef_file", build_env_info.classdef_file)
                .add("--system_header", system_info.system_header)
                .add("--game_header", build_env_info.game_header)
                .add_all(include_dirs, before_each = "-I")
                .add("-o", out_dir.path)
                .add_all(srcs),
        ],
    )

    return [
        DefaultInfo(
            files = depset(outputs),
        ),
    ]

sci_binary = rule(
    implementation = _sci_binary_impl,
    attrs = {
        "srcs": attr.label_list(providers = [_SciScriptInfo], mandatory = True),
        "build_env": attr.label(providers = [_SciBuildEnvInfo], mandatory = True),
        "_sc_binary": attr.label(executable = True, default = Label("//src:sc"), cfg = "exec"),
    },
    toolchains = ["//toolchain:sci_compiler_toolchain_type"],
)
