"""Rules to build with the SC compiler."""

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
        "system_header": "A depset of system headers to include.",
        "game_header": "A depset of game headers to include.",
        "target_vm": "The target VM to use, as a string.",
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

def _sci_build_env_impl(ctx):
    return [
        _SciBuildEnvInfo(
            selector_file = ctx.file.selector,
            classdef_file = ctx.file.classdef,
            system_header = ctx.file.system_header,
            game_header = ctx.file.game_header,
            target_vm = ctx.attr.target_vm,
        ),
    ]

sci_build_env = rule(
    implementation = _sci_build_env_impl,
    attrs = {
        "selector": attr.label(allow_single_file = True, mandatory = True),
        "classdef": attr.label(allow_single_file = True),
        "system_header": attr.label(allow_single_file = [".sh"]),
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
        ])
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
    build_env_info = ctx.attr.build_env[_SciBuildEnvInfo]
    out_dir = ctx.actions.declare_directory(ctx.label.name)
    outputs = [out_dir]
    inputs = [
        build_env_info.selector_file,
        build_env_info.classdef_file,
        build_env_info.system_header,
        build_env_info.game_header,
    ]
    srcs = []
    hdrs = depset(transitive = [
        script[_SciScriptInfo].headers
        for script in ctx.attr.srcs
    ]).to_list()
    include_dirs = [hdr.dirname for hdr in hdrs]
    for src_value in ctx.attr.srcs:
        src_info = src_value[_SciScriptInfo]
        srcs.append(src_info.src)

    if build_env_info.target_vm == "2":
        target_env = "SCI_2"
    elif build_env_info.target_vm == "1.1":
        target_env = "SCI_1_1"
    else:
        fail("Unknown target VM: {0}".format(build_env_info.target_vm))

    ctx.actions.run(
        inputs = inputs + srcs + hdrs,
        outputs = outputs,
        executable = ctx.executable._sc_binary,
        arguments = [
            ctx.actions.args()
                .add("-a")
                .add("-u")
                .add("-D", target_env)
                .add("-t", target_env)
                .add("--selector_file", build_env_info.selector_file)
                .add("--classdef_file", build_env_info.classdef_file)
                .add("--system_header", build_env_info.system_header)
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
)
